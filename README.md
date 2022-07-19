# StyleTransfer_UE4
realtime style transfer in unreal engine

## Steps for Release build 

* Make Sure only enable one Intel iGPU 

* Right click `StyleTransfer.uproject`, click `generate visual studio project file`, then `StyleTransfer.sln` is appeared

* Open `StyleTransfer.sln`, select `Development Editor` to build

* Open `StyleTransfer.uproject`, go to menu `File-> Project Launcher -> Windows(64-bit)`

* Choose `folder\for\Release project`, waiting for the compiling result

* Go to `folder\for\Release project`, run `StyleTransfer.exe -WINDOWED -ResX=1920 -ResY=1080 -ExecCmds="r.OVST.Width 1920, r.OVST.Height 1080"`
  ![Result](doc/Result_manga.png)

* Press `~` to change mode, for example, `r.OVST.Enabled 2`.  

  ## Performance

### model_v5 inference

|          | FPS  | Inference time |  copy buffer from CPU to GPU| Loading time |
|----------|------|----------------|---------------|               -------------|
| No style | 200  | 0              |  0            |               0            |
| GPU_OCL  | 9    | 105ms          |  0            |               3.8s         |
| GPU1     | 6    | 150ms          |  17ms         |               2.8s         |
| GPU0     | 1.89 | 502ms          |  18ms            |            3.1s         |

### model_v9 inference
|          | FPS  | Inference time |  copy buffer from CPU to GPU | Loading time |
|----------|------|----------------|---------------|                -------------|
| No style | 200  | 0              |  0            |                0            |
| GPU_OCL  | 24   | 38ms           |  0            |                3.2s         |
| GPU1     | 9.6   | 84ms          |  19ms         |                2.4s         |
| GPU0     | 3.4 | 256ms           |  23ms           |              2.0s         |

### model_v9_int8 inference

|          | FPS  | Inference time |  copy buffer from CPU to GPU |   Loading time |
|----------|------|----------------|---------------|                  -------------|
| No style | 200  | 0              |  0            |                  0            |
| GPU_OCL  | 31    | 28ms          |  0            |                  2.2s         |
| GPU1     | 10   | 76ms           |  19ms         |                  1.7s         |
| GPU0     | 5 | 156ms             |  23ms           |                1.1s         |

* GPU 0: Intel(R) UHD Graphics
* GPU 1: Intel(R) Arc(TM) A730M Graphics
* There are two times of CPU/GPU copy during inference in GPU.1/GPU.0 mode
* int8 model got 1.3x faster on GPU.1 and 2x faster on GPU.0  

## To-Do list
  - [x] Change model to int8 precision for 30 fps target
  - [ ] GPU管线和openvino 推理 时间统计，加到屏幕统计信息 
  - [x] 用[openvino D3D api](https://docs.openvino.ai/2021.4/classInferenceEngine_1_1gpu_1_1D3DBufferBlob.html) 拿到DirectX的数据做推理
  - [x] 把UE渲染数据通过`ID3D11Device`的方式拿到
  - [x] 把style transfer的结果放到主窗口显示 
  

## Step to build OpenVINO Wrapper
* `mkdir build`
*  `cd build`
* `cmake ..`
* open `OpenVinoWrapper.sln` project properties -> C/C++ -> preprocessor -> preprocessor definition -> join NOMINMAX
