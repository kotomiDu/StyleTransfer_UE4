# StyleTransfer_UE4
realtime style transfer in unreal engine

## Steps for Release build 

* Make Sure only enable one Intel iGPU 

* Right click `StyleTransfer.uproject`, click `generate visual studio project file`, then `StyleTransfer.sln` is appeared

* Open `StyleTransfer.sln`, select `Development Editor` to build

* Open `StyleTransfer.uproject`, go to menu `File-> Project Launcher -> Windows(64-bit)`

* Choose `folder\for\Release project`, waiting for the compiling result

* Go to `folder\for\Release project`, run `StyleTransfer.exe -WINDOWED -ResX=512 -ResY=512 -ExecCmds="r.OVST.Width 512, r.OVST.Height 512"`
  ![Result](doc/Result_manga.png)

* Press `~` to change mode, for example, `r.OVST.Enabled 2`.  

  ## Performance

### CPU inference

|         | FPS  | 12900K CPU Usage | 3080 GPU Usage |
| ------- | ---- | ---------------- | -------------- |
| 512*512 | 9   | 42%              | 32%            |
| 224*224 | 40   | 32%              | 42%            |

|         | FPS  | 11900K CPU Usage | DG2 128EU      |
| ------- | ---- | ---------------- | -------------- |
| 512*512 | 17   | 55%              | 7%             |
| 224*224 | 105  | 52%              | 27%            |

### iGPU inference

|         | FPS  | 12900K CPU Usage | UHDGraphcis770 |3080 GPU Usage |
| ------- | ---- | ---------------- | -------------- |-------------- |
| 512*512 | 11   | 12%              | 90%            |27%            |
| 224*224 | 40   | 20%              | 62%            |39%            |


### DG2 GPU inference

|         | FPS  | 11900K CPU Usage  |DG2 128EU      |
| ------- | ---- | ----------------  |-------------- |
| 512*512 | 82   | 29%               |20%            |
| 224*224 | 148  | 25%               |23%            |

## To-Do list
  - [ ] GPU管线和openvino 推理 时间统计，加到屏幕统计信息 
  - [ ] 用[openvino D3D api](https://docs.openvino.ai/2021.4/classInferenceEngine_1_1gpu_1_1D3DBufferBlob.html) 拿到DirectX的数据做推理
  - [ ] 是否可以把UE渲染数据通过`ID3D11Device`的方式拿到
  - [ ] 把style transfer的结果放到主窗口显示 
  

## Step to build OpenVINO Wrapper
* `mkdir build`
*  `cd build`
* `cmake ..`
* open `OpenVinoWrapper.sln` project properties -> C/C++ -> preprocessor -> preprocessor definition -> join NOMINMAX
