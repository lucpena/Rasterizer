#include <iostream>
#include <vector>
#include <algorithm>

#include "tgaimage.h"
#include "model.h"
#include "geometry.h"
#include "our_gl.h"

const TGAColor white = TGAColor(255, 255, 255, 255);
const TGAColor red   = TGAColor(255, 0, 0, 255);
const TGAColor green = TGAColor(0, 255, 0, 255);
const TGAColor blue  = TGAColor(0, 0, 255, 255);

Model *model = NULL;
const int width = 1024;
const int height = 1024;

Vec3f light_dir(1, 1, 1);
Vec3f       eye(1, 1, 3);
Vec3f    center(0, 0, 0);
Vec3f        up(0, 1, 0);

struct GouraudShader : public IShader {
    // Written by vertex shader
    // Read by fragment shader
    Vec3f varying_intensity;

    virtual Vec4f vertex(int iface, int nthvert){
        // Get diffuse lighting Intensity
        varying_intensity[nthvert] = std::max(0.f, model->normal(iface, nthvert) * light_dir);

        // Read the vertex from the .obj file
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));

        // Transform it to screen coordinates
        return Viewport * Projection * ModelView * gl_Vertex;
    }

    virtual bool fragment(Vec3f bar, TGAColor &color){
        // Interpolate intensity for the current pixel
        float intensity = varying_intensity * bar;
        color = TGAColor(255, 255, 255) * intensity;

        // Do not discard this pixel
        return false;
    }
};

// Shader with the Textures
struct Shader : public IShader {
    mat<2, 3, float> varying_uv;

    // Projection * ModelView
    mat<4, 4, float> uniform_M;

    // (Projection * ModelView).invert_transpose()
    mat<4, 4, float> uniform_MIT;

    virtual Vec4f vertex(int iface, int nthvert){
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));       

        // Read the vertex from .obj file
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));

        // transform it to the screen coordinates
        return Viewport * Projection * ModelView * gl_Vertex;
    }

    virtual bool fragment(Vec3f bar, TGAColor &color){
        // Interpolate uv for the current pixel
        Vec2f uv = varying_uv * bar;
        Vec3f n = proj<3>(uniform_MIT * embed<4>(model->normal(uv))).normalize();
        Vec3f l = proj<3>(uniform_M   * embed<4>(light_dir)).normalize();

        // Reflected Light
        Vec3f r = (n * (n * l * 2.f) - l).normalize();

        float spec = pow(std::max(r.z, 0.0f), model->specular(uv));
        float diff = std::max(0.f, n * l);
        TGAColor c = model->diffuse(uv);
        color = c;

        // 5 -> Ambient Component
        // 1 -> Diffuse Component
        // .6 -> Specular Component
        for(int i = 0; i < 3; i++){
            color[i] = std::min<float>(5 + c[i] * (diff + .6 * spec), 255);
        }

        // Do not discard this pixel
        return false;
    }
};


int main(int argc, char** argv){
    std::cout << "Gerando imagem. Agurade..." << std::endl;

    if (2 == argc){
        model = new Model(argv[1]);
    } else {
        model = new Model("obj/diablo/diablo3_pose.obj");
    }

    lookat(eye, center, up);
    viewport(width / 8, height / 8, width * 3 / 4, height * 3 / 4);
    projection(-1.f / (eye - center).norm());
    light_dir.normalize();

    TGAImage   image(width, height, TGAImage::RGB);
    TGAImage zbuffer(width, height, TGAImage::GRAYSCALE);

    Shader shader;
    shader.uniform_M = Projection * ModelView;
    shader.uniform_MIT = (Projection * ModelView).invert_transpose();

    for(int i = 0; i < model->nfaces(); i++){
        Vec4f screen_coords[3];
        for(int j = 0; j < 3; j++){
            screen_coords[j] = shader.vertex(i, j);
        }

        triangle(screen_coords, shader, image, zbuffer);
    }

    // Places the origin to the bottom left corner
    image.flip_vertically();
    zbuffer.flip_vertically();

    image.write_tga_file("output.tga");
    zbuffer.write_tga_file("zbuffer.tga");

    delete model;
    std::cout << "Imagem gerada com sucesso. Checar 'output.tga'." << std::endl;
    return 0;
}