// Opengl_objview2.cpp : アプリケーションのエントリ ポイントを定義します。
//

#include "pch.h"
#include "framework.h"
#include "Opengl_objview2.h"

#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <stdexcept>
#include <iostream> // デバッグ用
#include <algorithm> // std::replace 用
#include <map> // マテリアル用

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")

// ウィンドウプロシージャの宣言
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// グローバル変数
HDC hDC;
HGLRC hRC;
HWND hWnd;
HINSTANCE hInstance;

// マテリアル情報を格納する構造体
struct Material {
    float diffuse[3];
};

// OBJファイルを扱うクラス
class OBJModel {
public:
    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<int> faces;
    std::vector<int> normalIndices;
    std::map<std::string, Material> materials;
    std::vector<std::string> faceMaterials; // 各面のマテリアル名
    std::string currentMaterial;

    OBJModel() {
        // 初期化
        vertices.clear();
        normals.clear();
        faces.clear();
        normalIndices.clear();
        materials.clear();
        faceMaterials.clear();
        currentMaterial.clear();
        OutputDebugStringA("OBJModel constructed and initialized.\n");
    }

    bool Load(const char* filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            OutputDebugStringA(("Failed to open OBJ file: " + std::string(filename) + "\n").c_str());
            return false;
        }

        std::string line;
        while (std::getline(file, line)) {
            std::istringstream s(line);
            std::string type;
            s >> type;
            if (type == "v") {
                float x, y, z;
                if (!(s >> x >> y >> z)) {
                    OutputDebugStringA("Error reading vertex data.\n");
                    continue;
                }
                vertices.push_back(x);
                vertices.push_back(y);
                vertices.push_back(z);
            }
            else if (type == "vn") {
                float nx, ny, nz;
                if (!(s >> nx >> ny >> nz)) {
                    OutputDebugStringA("Error reading normal data.\n");
                    continue;
                }
                normals.push_back(nx);
                normals.push_back(ny);
                normals.push_back(nz);
            }
            else if (type == "f") {
                std::string vertexData;
                std::vector<int> faceIndices;
                std::vector<int> faceNormalIndices;
                while (s >> vertexData) {
                    std::replace(vertexData.begin(), vertexData.end(), '/', ' ');
                    std::istringstream vertexStream(vertexData);
                    int vIdx, tIdx, nIdx;
                    vertexStream >> vIdx;
                    if (vertexStream.peek() == ' ') {
                        vertexStream >> tIdx;
                    }
                    if (vertexStream >> nIdx) {
                        faceNormalIndices.push_back(nIdx - 1);
                    }
                    faceIndices.push_back(vIdx - 1);
                }
                // 三角形分割処理（ポリゴンを三角形に分割）
                for (size_t i = 1; i < faceIndices.size() - 1; ++i) {
                    faces.push_back(faceIndices[0]);
                    faces.push_back(faceIndices[i]);
                    faces.push_back(faceIndices[i + 1]);
                    faceMaterials.push_back(currentMaterial); // 各面のマテリアル名を保存
                    if (!faceNormalIndices.empty()) {
                        normalIndices.push_back(faceNormalIndices[0]);
                        normalIndices.push_back(faceNormalIndices[i]);
                        normalIndices.push_back(faceNormalIndices[i + 1]);
                    }
                }
            }
            else if (type == "mtllib") {
                std::string mtlFilename;
                s >> mtlFilename;
                LoadMTL(mtlFilename.c_str());
            }
            else if (type == "usemtl") {
                s >> currentMaterial;
                OutputDebugStringA(("Using material: " + currentMaterial + "\n").c_str());
            }
        }

        file.close();
        if (vertices.empty() || faces.empty()) {
            OutputDebugStringA("No valid vertex or face data found in OBJ file.\n");
            return false;
        }

        // デバッグ情報: 読み込んだ頂点と面の数を表示
        OutputDebugStringA(("Loaded " + std::to_string(vertices.size() / 3) + " vertices and " + std::to_string(faces.size() / 3) + " faces.\n").c_str());

        return true;
    }

    bool LoadMTL(const char* filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            OutputDebugStringA(("Failed to open MTL file: " + std::string(filename) + "\n").c_str());
            return false;
        }

        std::string line;
        std::string currentMtlName;
        while (std::getline(file, line)) {
            std::istringstream s(line);
            std::string type;
            s >> type;
            if (type == "newmtl") {
                s >> currentMtlName;
                materials[currentMtlName] = Material();
                OutputDebugStringA(("Defined new material: " + currentMtlName + "\n").c_str());
            }
            else if (type == "Kd") {
                float r, g, b;
                s >> r >> g >> b;
                if (!currentMtlName.empty()) {
                    materials[currentMtlName].diffuse[0] = r;
                    materials[currentMtlName].diffuse[1] = g;
                    materials[currentMtlName].diffuse[2] = b;
                    OutputDebugStringA(("Material " + currentMtlName + " diffuse color set to: " + std::to_string(r) + ", " + std::to_string(g) + ", " + std::to_string(b) + "\n").c_str());
                }
            }
        }

        file.close();
        return true;
    }

    void Draw() {
        glBegin(GL_TRIANGLES);
        for (size_t i = 0; i < faces.size(); i += 3) {
            int idx1 = faces[i];
            int idx2 = faces[i + 1];
            int idx3 = faces[i + 2];

            // インデックスが有効かどうかをチェック
            if (idx1 < 0 || idx1 * 3 + 2 >= vertices.size() ||
                idx2 < 0 || idx2 * 3 + 2 >= vertices.size() ||
                idx3 < 0 || idx3 * 3 + 2 >= vertices.size()) {
                OutputDebugStringA(("Invalid face indices: " + std::to_string(idx1) + ", " + std::to_string(idx2) + ", " + std::to_string(idx3) + "\n").c_str());
                continue; // 無効なインデックスの場合はスキップ
            }

            // マテリアルの色を設定
            std::string materialName = faceMaterials[i / 3];
            if (!materialName.empty() && materials.find(materialName) != materials.end()) {
                Material& mat = materials[materialName];
                glColor3f(mat.diffuse[0], mat.diffuse[1], mat.diffuse[2]);
            }

            // 法線を設定
            if (!normalIndices.empty()) {
                int nIdx1 = normalIndices[i];
                int nIdx2 = normalIndices[i + 1];
                int nIdx3 = normalIndices[i + 2];
                if (nIdx1 >= 0 && nIdx1 * 3 + 2 < normals.size()) {
                    glNormal3f(normals[nIdx1 * 3], normals[nIdx1 * 3 + 1], normals[nIdx1 * 3 + 2]);
                }
            }

            glVertex3f(vertices[idx1 * 3], vertices[idx1 * 3 + 1], vertices[idx1 * 3 + 2]);
            glVertex3f(vertices[idx2 * 3], vertices[idx2 * 3 + 1], vertices[idx2 * 3 + 2]);
            glVertex3f(vertices[idx3 * 3], vertices[idx3 * 3 + 1], vertices[idx3 * 3 + 2]);
        }
        glEnd();
    }
};

// 初期化関数
void InitOpenGL() {
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,
        32,
        0, 0, 0, 0, 0, 0,
        0,
        0,
        0,
        0, 0, 0, 0,
        24,
        8,
        0,
        PFD_MAIN_PLANE,
        0,
        0, 0, 0
    };

    hDC = GetDC(hWnd);
    int format = ChoosePixelFormat(hDC, &pfd);
    SetPixelFormat(hDC, format, &pfd);
    hRC = wglCreateContext(hDC);
    wglMakeCurrent(hDC, hRC);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING); // ライティングを有効化
    glEnable(GL_LIGHT0);   // ライト0を有効化

    // ライトの設定
    GLfloat light_position[] = { 1.0f, 1.0f, 1.0f, 0.0f };
    GLfloat light_ambient[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    GLfloat light_diffuse[] = { 0.8f, 0.8f, 0.8f, 1.0f };
    GLfloat light_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };

    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);

    // 投影行列の設定
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, 800.0 / 600.0, 1.0, 10000.0); // 遠いクリッピング平面を大きく設定
    glMatrixMode(GL_MODELVIEW);

    //色処理
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

}

// 描画関数
void RenderScene(OBJModel& objModel) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    gluLookAt(5000.0f, 5000.0f, 5000.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f); // カメラ位置を遠くに設定

    // OBJデータを描画
    objModel.Draw();

    SwapBuffers(hDC);
}

// WinMainエントリーポイント
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASS wc;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = L"OpenGLWinClass";

    if (!RegisterClass(&wc)) {
        return 0;
    }

    hWnd = CreateWindow(
        L"OpenGLWinClass",
        L"OpenGL OBJ Viewer",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        NULL, NULL,
        hInstance, NULL
    );

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    InitOpenGL();

    OBJModel objModel;
    // OBJファイルの読み込み
    if (!objModel.Load("Forklift_simple.obj")) {
        MessageBox(NULL, L"Failed to load OBJ file!", L"Error", MB_OK | MB_ICONERROR);
        return 0;
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        RenderScene(objModel);
    }

    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hRC);
    ReleaseDC(hWnd, hDC);

    return msg.wParam;
}

// ウィンドウプロシージャ
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
