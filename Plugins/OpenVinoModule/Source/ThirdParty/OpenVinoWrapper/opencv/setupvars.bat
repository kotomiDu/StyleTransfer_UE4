setlocal
set _OPENVINO_OPENCV_ROOT=%~dp0
endlocal & (
  set "OpenCV_DIR=%_OPENVINO_OPENCV_ROOT%\cmake"
  set "PATH=%_OPENVINO_OPENCV_ROOT%\bin;%PATH%"
  set "PYTHONPATH=%_OPENVINO_OPENCV_ROOT%\python;%PYTHONPATH%"
)
