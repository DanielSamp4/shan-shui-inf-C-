#include "scroll.hpp"

#include "canvas.hpp"
#include "generator.hpp"
#include "raster.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <windows.h>

namespace {

constexpr int kViewWidth = 3000;
constexpr int kViewHeight = 800;
constexpr double kWorldScale = kRasterWorldScale;
constexpr double kCameraStart = 0;
constexpr double kForgetMargin = 1600;
// Keep several chunks ready beyond the right edge.  At 180 px/s this gives the
// worker more than twenty seconds to generate and rasterize scenery before it
// can enter the viewport.
constexpr double kLookAhead = 4096;
constexpr Rgba kPaper{245, 236, 220, 1};
// Normal VSync is about 16.7 ms at 60 Hz. Log only noticeably late frames so
// the diagnostic output does not itself become a source of animation stalls.
constexpr double kStutterThresholdMs = 28.0;

struct QuadConstants {
  float left = 0;
  float top = 0;
  float width = 0;
  float height = 0;
};

struct QuadVertex {
  float x;
  float y;
  float u;
  float v;
};

using Sprite = RasterizedPiece;

void compose(Canvas& frame, const std::vector<Sprite>& sprites, double camera, const Canvas& paper) {
  tilePaper(frame, paper);
  Canvas ink = Canvas::transparent(frame.width(), frame.height());
  for (const Sprite& sprite : sprites) {
    const int x = static_cast<int>(std::lround(sprite.left - camera * kWorldScale));
    const int y = static_cast<int>(std::lround(sprite.top));
    ink.blit(x, y, sprite.image);
  }
  frame.blendMultiply(ink);
}

LRESULT CALLBACK scrollProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  if (message == WM_CLOSE) {
    DestroyWindow(window);
    return 0;
  }
  if (message == WM_DESTROY) {
    PostQuitMessage(0);
    return 0;
  }
  if (message == WM_KEYDOWN && wparam == VK_ESCAPE) {
    DestroyWindow(window);
    return 0;
  }
  return DefWindowProc(window, message, wparam, lparam);
}

template <typename T>
void release(T*& object) {
  if (object != nullptr) {
    object->Release();
    object = nullptr;
  }
}

ID3D11ShaderResourceView* makeTexture(ID3D11Device* device, const Canvas& canvas) {
  std::vector<std::uint8_t> pixels(static_cast<std::size_t>(canvas.width() * canvas.height() * 4));
  for (int y = 0; y < canvas.height(); ++y) {
    for (int x = 0; x < canvas.width(); ++x) {
      const Rgba color = canvas.pixel(x, y);
      const std::size_t at = static_cast<std::size_t>((y * canvas.width() + x) * 4);
      const std::uint8_t alpha = static_cast<std::uint8_t>(std::lround(color.a * 255.0));
      // Premultiplied RGBA retains white ink. The multiply shader, rather than
      // alpha blending, decides how every colour affects the paper.
      pixels[at] = static_cast<std::uint8_t>(color.r * alpha / 255);
      pixels[at + 1] = static_cast<std::uint8_t>(color.g * alpha / 255);
      pixels[at + 2] = static_cast<std::uint8_t>(color.b * alpha / 255);
      pixels[at + 3] = alpha;
    }
  }
  D3D11_TEXTURE2D_DESC desc{};
  desc.Width = static_cast<UINT>(canvas.width());
  desc.Height = static_cast<UINT>(canvas.height());
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_IMMUTABLE;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  D3D11_SUBRESOURCE_DATA data{};
  data.pSysMem = pixels.data();
  data.SysMemPitch = static_cast<UINT>(canvas.width() * 4);
  ID3D11Texture2D* texture = nullptr;
  ID3D11ShaderResourceView* view = nullptr;
  if (FAILED(device->CreateTexture2D(&desc, &data, &texture)) ||
      FAILED(device->CreateShaderResourceView(texture, nullptr, &view))) {
    release(texture);
    return nullptr;
  }
  texture->Release();
  return view;
}

HRESULT compileShader(const char* source, const char* entry, const char* profile, ID3DBlob** blob) {
  ID3DBlob* errors = nullptr;
  const HRESULT result = D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr, entry, profile, 0, 0, blob,
                                    &errors);
  if (FAILED(result) && errors != nullptr) {
    std::cerr << static_cast<const char*>(errors->GetBufferPointer()) << "\n";
  }
  release(errors);
  return result;
}

void logStutter(const char* phase, double milliseconds) {
  if (milliseconds >= kStutterThresholdMs) {
    std::cout << "stutter " << phase << " " << milliseconds << " ms\n";
  }
}

}  // namespace

bool playScroll(std::vector<RasterizedPiece> sprites, const std::string& seed, double speed, Generator* live) {
  std::stable_sort(sprites.begin(), sprites.end(),
                   [](const Sprite& a, const Sprite& b) { return a.sortY < b.sortY; });

  const Canvas& paper = paperTile(seed);
  Canvas first(kViewWidth, kViewHeight, kPaper);
  compose(first, sprites, kCameraStart, paper);
  Canvas later(kViewWidth, kViewHeight, kPaper);
  compose(later, sprites, kCameraStart + speed, paper);
  const bool moved = first.checksum() != later.checksum();
  const bool scrollOk = speed == 0.0 ? !moved : moved;
  first.writeBmp("scroll-check.bmp");
  std::cout << (scrollOk ? "scroll match\n" : "scroll mismatch\n");
  std::cout << "ok scroll-check.bmp\n";

  WNDCLASSW windowClass{};
  windowClass.lpfnWndProc = scrollProc;
  windowClass.hInstance = GetModuleHandleW(nullptr);
  windowClass.lpszClassName = L"ShanShuiScroll";
  windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
  RegisterClassW(&windowClass);

  RECT bounds{0, 0, kViewWidth, kViewHeight};
  AdjustWindowRect(&bounds, WS_OVERLAPPEDWINDOW, FALSE);
  HWND window = CreateWindowW(L"ShanShuiScroll", L"shan shui", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT,
                              CW_USEDEFAULT, bounds.right - bounds.left, bounds.bottom - bounds.top, nullptr, nullptr,
                              windowClass.hInstance, nullptr);
  if (window == nullptr) {
    std::cout << "janela indisponivel\n";
    return scrollOk;
  }

  ID3D11Device* device = nullptr;
  ID3D11DeviceContext* context = nullptr;
  D3D_FEATURE_LEVEL featureLevel{};
  if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
                               D3D11_SDK_VERSION, &device, &featureLevel, &context))) {
    std::cout << "Direct3D indisponivel\n";
    DestroyWindow(window);
    return false;
  }
  IDXGIDevice* dxgiDevice = nullptr;
  IDXGIAdapter* adapter = nullptr;
  IDXGIFactory2* factory = nullptr;
  IDXGISwapChain1* swapChain = nullptr;
  if (FAILED(device->QueryInterface(&dxgiDevice)) || FAILED(dxgiDevice->GetAdapter(&adapter)) ||
      FAILED(adapter->GetParent(IID_PPV_ARGS(&factory)))) {
    release(factory); release(adapter); release(dxgiDevice); release(context); release(device);
    DestroyWindow(window);
    return false;
  }
  DXGI_SWAP_CHAIN_DESC1 swapDesc{};
  swapDesc.Width = kViewWidth;
  swapDesc.Height = kViewHeight;
  swapDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  swapDesc.SampleDesc.Count = 1;
  swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapDesc.BufferCount = 2;
  swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  if (FAILED(factory->CreateSwapChainForHwnd(device, window, &swapDesc, nullptr, nullptr, &swapChain))) {
    release(factory); release(adapter); release(dxgiDevice); release(context); release(device);
    DestroyWindow(window);
    return false;
  }
  release(factory); release(adapter); release(dxgiDevice);

  ID3D11Texture2D* backBuffer = nullptr;
  ID3D11RenderTargetView* backRtv = nullptr;
  if (FAILED(swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))) ||
      FAILED(device->CreateRenderTargetView(backBuffer, nullptr, &backRtv))) {
    release(backRtv); release(backBuffer); release(swapChain); release(context); release(device);
    DestroyWindow(window);
    return false;
  }
  release(backBuffer);

  D3D11_TEXTURE2D_DESC inkDesc{};
  inkDesc.Width = kViewWidth;
  inkDesc.Height = kViewHeight;
  inkDesc.MipLevels = 1;
  inkDesc.ArraySize = 1;
  inkDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  inkDesc.SampleDesc.Count = 1;
  inkDesc.Usage = D3D11_USAGE_DEFAULT;
  inkDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
  ID3D11Texture2D* inkTexture = nullptr;
  ID3D11RenderTargetView* inkRtv = nullptr;
  ID3D11ShaderResourceView* inkSrv = nullptr;
  if (FAILED(device->CreateTexture2D(&inkDesc, nullptr, &inkTexture)) ||
      FAILED(device->CreateRenderTargetView(inkTexture, nullptr, &inkRtv)) ||
      FAILED(device->CreateShaderResourceView(inkTexture, nullptr, &inkSrv))) {
    release(inkSrv); release(inkRtv); release(inkTexture); release(backRtv); release(swapChain); release(context); release(device);
    DestroyWindow(window);
    return false;
  }
  release(inkTexture);

  static const char* kVertexShader =
      "cbuffer Quad : register(b0) { float4 rect; };"
      "struct In { float2 pos:POSITION; float2 uv:TEXCOORD; };"
      "struct Out { float4 pos:SV_POSITION; float2 uv:TEXCOORD; };"
      "Out main(In i) { Out o; float2 p=rect.xy+i.pos*rect.zw; o.pos=float4(p.x/1500.0-1.0,1.0-p.y/400.0,0,1); o.uv=i.uv; return o; }";
  static const char* kSpriteShader =
      "Texture2D image:register(t0); SamplerState sampler0:register(s0);"
      "float4 main(float4 pos:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET { return image.Sample(sampler0,uv); }";
  static const char* kMultiplyShader =
      "Texture2D paper:register(t0); Texture2D ink:register(t1); SamplerState sampler0:register(s0);"
      "float4 main(float4 pos:SV_POSITION,float2 uv:TEXCOORD):SV_TARGET { float4 p=paper.Sample(sampler0,uv); float4 i=ink.Sample(sampler0,uv); return float4(p.rgb*(1.0-i.a+i.rgb),1.0); }";
  ID3DBlob* vsBlob = nullptr;
  ID3DBlob* spriteBlob = nullptr;
  ID3DBlob* multiplyBlob = nullptr;
  ID3D11VertexShader* vertexShader = nullptr;
  ID3D11PixelShader* spriteShader = nullptr;
  ID3D11PixelShader* multiplyShader = nullptr;
  ID3D11InputLayout* inputLayout = nullptr;
  if (FAILED(compileShader(kVertexShader, "main", "vs_4_0", &vsBlob)) ||
      FAILED(compileShader(kSpriteShader, "main", "ps_4_0", &spriteBlob)) ||
      FAILED(compileShader(kMultiplyShader, "main", "ps_4_0", &multiplyBlob)) ||
      FAILED(device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader)) ||
      FAILED(device->CreatePixelShader(spriteBlob->GetBufferPointer(), spriteBlob->GetBufferSize(), nullptr, &spriteShader)) ||
      FAILED(device->CreatePixelShader(multiplyBlob->GetBufferPointer(), multiplyBlob->GetBufferSize(), nullptr, &multiplyShader))) {
    release(multiplyBlob); release(spriteBlob); release(vsBlob); release(multiplyShader); release(spriteShader); release(vertexShader);
    release(inkSrv); release(inkRtv); release(backRtv); release(swapChain); release(context); release(device); DestroyWindow(window); return false;
  }
  const D3D11_INPUT_ELEMENT_DESC layout[] = {{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
                                               {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0}};
  if (FAILED(device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout))) {
    release(multiplyBlob); release(spriteBlob); release(vsBlob); release(multiplyShader); release(spriteShader); release(vertexShader);
    release(inkSrv); release(inkRtv); release(backRtv); release(swapChain); release(context); release(device); DestroyWindow(window); return false;
  }
  release(multiplyBlob); release(spriteBlob); release(vsBlob);

  const QuadVertex vertices[] = {{0, 0, 0, 0}, {1, 0, 1, 0}, {0, 1, 0, 1}, {1, 1, 1, 1}};
  D3D11_BUFFER_DESC vertexDesc{};
  vertexDesc.ByteWidth = sizeof(vertices);
  vertexDesc.Usage = D3D11_USAGE_IMMUTABLE;
  vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  D3D11_SUBRESOURCE_DATA vertexData{};
  vertexData.pSysMem = vertices;
  ID3D11Buffer* vertexBuffer = nullptr;
  D3D11_BUFFER_DESC constantDesc{};
  constantDesc.ByteWidth = sizeof(QuadConstants);
  constantDesc.Usage = D3D11_USAGE_DEFAULT;
  constantDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  ID3D11Buffer* constantBuffer = nullptr;
  D3D11_SAMPLER_DESC samplerDesc{};
  samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
  samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
  samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
  samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
  ID3D11SamplerState* sampler = nullptr;
  D3D11_BLEND_DESC blendDesc{};
  blendDesc.RenderTarget[0].BlendEnable = TRUE;
  blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
  blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
  blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
  blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
  blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
  blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
  blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
  ID3D11BlendState* sourceOver = nullptr;
  if (FAILED(device->CreateBuffer(&vertexDesc, &vertexData, &vertexBuffer)) ||
      FAILED(device->CreateBuffer(&constantDesc, nullptr, &constantBuffer)) ||
      FAILED(device->CreateSamplerState(&samplerDesc, &sampler)) ||
      FAILED(device->CreateBlendState(&blendDesc, &sourceOver))) {
    std::cout << "recursos graficos indisponiveis\n";
    DestroyWindow(window);
    return false;
  }

  Canvas paperFrame(kViewWidth, kViewHeight, kPaper);
  tilePaper(paperFrame, paper);
  ID3D11ShaderResourceView* paperTexture = makeTexture(device, paperFrame);
  std::vector<ID3D11ShaderResourceView*> textures;
  textures.reserve(sprites.size());
  for (const Sprite& sprite : sprites) textures.push_back(makeTexture(device, sprite.image));

  const UINT stride = sizeof(QuadVertex);
  const UINT offset = 0;
  const D3D11_VIEWPORT viewport{0, 0, static_cast<float>(kViewWidth), static_cast<float>(kViewHeight), 0, 1};
  context->IASetInputLayout(inputLayout);
  context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  context->VSSetShader(vertexShader, nullptr, 0);
  context->VSSetConstantBuffers(0, 1, &constantBuffer);
  context->PSSetSamplers(0, 1, &sampler);
  context->RSSetViewports(1, &viewport);
  double camera = kCameraStart;
  auto previous = std::chrono::steady_clock::now();
  bool quit = false;
  while (!quit) {
    const auto frameStarted = std::chrono::steady_clock::now();
    double pieceRasterMs = 0.0;
    double textureUploadMs = 0.0;
    double extendMs = 0.0;
    double cleanupMs = 0.0;
    MSG message;
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
      if (message.message == WM_QUIT) {
        quit = true;
        break;
      }
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
    if (quit) {
      break;
    }
    const double messageMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - frameStarted).count();

    // Same rule used by index.html's requestAnimationFrame loop: the camera
    // advances according to elapsed time and retains its fractional position.
    const auto now = std::chrono::steady_clock::now();
    const double dt = std::min(0.034, std::chrono::duration<double>(now - previous).count());
    previous = now;
    camera += speed * dt;
    if (live != nullptr) {
      const double requestedMax = camera + kViewWidth / kWorldScale + kLookAhead;
      if (live->exhausted() && requestedMax > live->plan().xmax()) {
        const auto extendStarted = std::chrono::steady_clock::now();
        // Extending only to plan().xmax() adds one 512-unit chunk.  Request
        // the complete horizon instead, so no object needs to be constructed
        // while it is becoming visible.
        live->extend(requestedMax);
        extendMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - extendStarted).count();
      }
      int drawn = 0;
      GenEvent event;
      while (drawn < 2 && live->poll(event)) {
        if (event.kind == GenEvent::Kind::Timing) {
          logStutter(event.tag, event.ms);
          continue;
        }
        Sprite sprite = std::move(event.raster);
        const auto at = std::lower_bound(sprites.begin(), sprites.end(), sprite.sortY,
                                          [](const Sprite& item, double sortY) { return item.sortY < sortY; });
        const std::size_t index = static_cast<std::size_t>(at - sprites.begin());
        const auto uploadStarted = std::chrono::steady_clock::now();
        ID3D11ShaderResourceView* texture = makeTexture(device, sprite.image);
        textureUploadMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - uploadStarted).count();
        sprites.insert(at, std::move(sprite));
        textures.insert(textures.begin() + static_cast<std::ptrdiff_t>(index), texture);
        drawn += 1;
      }
    }
    const auto cleanupStarted = std::chrono::steady_clock::now();
    const double limit = (camera - kForgetMargin) * kWorldScale;
    for (std::size_t i = 0; i < sprites.size();) {
      if (sprites[i].left + sprites[i].image.width() >= limit) {
        ++i;
        continue;
      }
      release(textures[i]);
      sprites.erase(sprites.begin() + static_cast<std::ptrdiff_t>(i));
      textures.erase(textures.begin() + static_cast<std::ptrdiff_t>(i));
    }
    cleanupMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - cleanupStarted).count();

    // Pass 1: compose all ink into a transparent texture with source-over,
    // matching the ordering used while the original Canvas was built.
    const float transparent[] = {0, 0, 0, 0};
    context->OMSetRenderTargets(1, &inkRtv, nullptr);
    context->ClearRenderTargetView(inkRtv, transparent);
    context->OMSetBlendState(sourceOver, nullptr, 0xffffffff);
    context->PSSetShader(spriteShader, nullptr, 0);
    for (std::size_t i = 0; i < sprites.size(); ++i) {
      if (textures[i] == nullptr) continue;
      const Sprite& sprite = sprites[i];
      const QuadConstants quad{static_cast<float>(sprite.left - camera * kWorldScale), static_cast<float>(sprite.top),
                               static_cast<float>(sprite.image.width()), static_cast<float>(sprite.image.height())};
      context->UpdateSubresource(constantBuffer, 0, nullptr, &quad, 0, 0);
      context->PSSetShaderResources(0, 1, &textures[i]);
      context->Draw(4, 0);
    }
    ID3D11ShaderResourceView* none[] = {nullptr, nullptr};
    context->PSSetShaderResources(0, 2, none);

    // Pass 2: browser-equivalent mix-blend-mode:multiply.
    context->OMSetRenderTargets(1, &backRtv, nullptr);
    context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    context->PSSetShader(multiplyShader, nullptr, 0);
    const QuadConstants full{0, 0, static_cast<float>(kViewWidth), static_cast<float>(kViewHeight)};
    context->UpdateSubresource(constantBuffer, 0, nullptr, &full, 0, 0);
    ID3D11ShaderResourceView* inputs[] = {paperTexture, inkSrv};
    context->PSSetShaderResources(0, 2, inputs);
    context->Draw(4, 0);
    context->PSSetShaderResources(0, 2, none);
    const auto presentStarted = std::chrono::steady_clock::now();
    if (FAILED(swapChain->Present(1, 0))) quit = true;
    const double presentMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - presentStarted).count();
    const double frameMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - frameStarted).count();
    if (frameMs >= kStutterThresholdMs) {
      // The split points distinguish GPU/VSync delay from new-piece work on
      // the UI thread. Generation timing above identifies worker contention.
      std::cout << "stutter frame " << frameMs << " ms | messages " << messageMs << " ms | extend " << extendMs
                << " ms | ui-raster " << pieceRasterMs << " ms | upload " << textureUploadMs << " ms | cleanup "
                << cleanupMs << " ms | present " << presentMs << " ms\n";
    }
  }
  for (ID3D11ShaderResourceView* texture : textures) release(texture);
  release(paperTexture); release(sourceOver); release(sampler); release(constantBuffer); release(vertexBuffer);
  release(inputLayout); release(multiplyShader); release(spriteShader); release(vertexShader);
  release(inkSrv); release(inkRtv); release(backRtv); release(swapChain); release(context); release(device);
  if (IsWindow(window)) {
    DestroyWindow(window);
  }
  return scrollOk;
}
