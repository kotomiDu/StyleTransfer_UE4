#pragma once
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#include <CL/opencl.h>
#include <CL/cl_d3d11.h>

#include <vector>
#include <map>
#include <string>
#include <mutex>

#define EXT_DECLARE(_name) _name##_fn _name


#define DEBUG_FLAG true
    class OCLEnv;

    class OCLProgram {
    public:
        OCLProgram(OCLEnv* env);
        virtual ~OCLProgram();
        bool Build(const std::string& buildSource, const std::string& buildOptions);
        cl_program GetHDL() { return m_program; }
    private:
        OCLEnv* m_env;
        cl_program m_program;
    };

    class OCLKernelArg;
    class OCLKernel {
    public:
        OCLKernel(OCLEnv* env);
        virtual ~OCLKernel();
        virtual bool Run() = 0;
        virtual bool Create(cl_program) = 0;

    protected:
        OCLEnv* m_env;
    };

    class OCLKernelArg {
    public:
        enum OCLKernelArgType {
            OCL_KERNEL_ARG_SURFACE,
            OCL_KERNEL_ARG_SHARED_SURFACE,
            OCL_KERNEL_ARG_INT,
            OCL_KERNEL_ARG_FLOAT
        };
        OCLKernelArg();
        virtual ~OCLKernelArg();
        virtual OCLKernelArgType Type() = 0;
        virtual bool Set(cl_kernel kernel) = 0;
        void SetIdx(cl_uint idx) { m_idx = idx; }
    protected:
        cl_uint m_idx;
    };

    class OCLEnv {
    public:
        enum OCLDevType {
            OCL_GPU_UNDEFINED,
            OCL_GPU_INTEGRATED,
            OCL_GPU_DISCRETE,
        };
        OCLEnv();
        virtual ~OCLEnv();

        bool Init(cl_platform_id clplatform);
        bool SetD3DDevice(ID3D11Device* device);
        cl_mem CreateSharedSurface(ID3D11Texture2D* surf, int nView, bool bIsReadOnly);
        bool EnqueueAcquireSurfaces(cl_mem* surfaces, int nSurfaces, bool flushAndFinish);
        bool EnqueueReleaseSurfaces(cl_mem* surfaces, int nSurfaces, bool flushAndFinish);

        bool EnqueueAcquireSurfaces(cl_command_queue command_queue,
            cl_uint num_objects,
            const cl_mem* mem_objects,
            cl_uint num_events_in_wait_list,
            const cl_event* event_wait_list,
            cl_event* event);

        bool EnqueueReleaseSurfaces(cl_command_queue command_queue,
            cl_uint num_objects,
            const cl_mem* mem_objects,
            cl_uint num_events_in_wait_list,
            const cl_event* event_wait_list,
            cl_event* event);

        //bool EnqueueAcquireAllSurfaces();
        //bool EnqueueReleaseAllSurfaces();

        EXT_DECLARE(clGetDeviceIDsFromD3D11KHR);
        EXT_DECLARE(clCreateFromD3D11Texture2DKHR);
        EXT_DECLARE(clEnqueueAcquireD3D11ObjectsKHR);
        EXT_DECLARE(clEnqueueReleaseD3D11ObjectsKHR);

        cl_device_id GetDevice() { return m_cldevice; }
        cl_platform_id GetPlatform() { return m_clplatform; }
        cl_context GetContext() { return m_clcontext; }
        cl_command_queue GetCommandQueue();

    private:
        OCLEnv(const OCLEnv&);

        ID3D11Device* m_d3d11device;
        cl_platform_id m_clplatform;
        cl_device_id   m_cldevice;
        cl_context     m_clcontext;
        cl_command_queue m_clqueue;
        OCLDevType     m_type;
        typedef std::pair<ID3D11Texture2D*, int> SurfaceKey;
        std::map<SurfaceKey, cl_mem> m_sharedSurfs;
        std::mutex m_sharedSurfMutex;
    };

    class OCL {
    public:
        OCL();
        virtual ~OCL() {}

        bool Init();
        std::shared_ptr<OCLEnv> GetEnv(ID3D11Device* dev);

    private:
        std::vector<std::shared_ptr<OCLEnv>> m_envs;
    };

    class OCLKernelArgSurface : public OCLKernelArg {
    public:
        OCLKernelArgSurface();
        virtual ~OCLKernelArgSurface();
        virtual OCLKernelArgType Type() { return OCL_KERNEL_ARG_SURFACE; }
        cl_mem GetHDL() { return m_hdl; }
        void SetHDL(cl_mem hdl) { m_hdl = hdl; }
        virtual bool Set(cl_kernel kernel);
    private:
        cl_mem m_hdl;
    };

    class OCLKernelArgSharedSurface : public OCLKernelArgSurface {
    public:
        OCLKernelArgSharedSurface() { ; }
        virtual ~OCLKernelArgSharedSurface() { ; }
        virtual OCLKernelArgType Type() { return OCL_KERNEL_ARG_SHARED_SURFACE; }
    };

    class OCLKernelArgInt : public OCLKernelArg {
    public:
        OCLKernelArgInt();
        ~OCLKernelArgInt();
        virtual OCLKernelArgType Type() { return OCL_KERNEL_ARG_INT; }
        virtual bool Set(cl_kernel kernel);
        void SetVal(cl_int val) { m_val = val; }
        cl_int GetVal() { return m_val; }
    private:
        cl_int m_val;
    };

    class OCLKernelArgFloat : public OCLKernelArg {
    public:
        OCLKernelArgFloat();
        ~OCLKernelArgFloat();
        virtual OCLKernelArgType Type() { return OCL_KERNEL_ARG_FLOAT; }
        virtual bool Set(cl_kernel kernel);
        void SetVal(cl_float val) { m_val = val; }
    private:
        cl_float m_val;
    };

    class SourceConversion : public OCLKernel {
    public:
        SourceConversion(OCLEnv* env);
        virtual ~SourceConversion();
        virtual bool Create(cl_program program);
        virtual bool Run();

        bool SetArgumentsRGBtoRGBbuffer(ID3D11Texture2D* in_nv12Surf, cl_mem out_rgbSurf, int cols, int rows);
        bool SetArgumentsRGBbuffertoRGBA(cl_mem in_rgbSurf, ID3D11Texture2D* out_rgbSurf, int cols, int rows);
        void printClVector(cl_mem& clVector, int length, cl_command_queue& commands, int datatype, int printrowlen = -1);
#if DEBUG_FLAG
        int surface_cols, surface_rows;
#endif
    private:
        cl_kernel   m_kernelRGBtoRGBbuffer;
        cl_kernel   m_kernelRGBbuffertoRGBA;

        bool m_RGBToRGBbuffer;
        size_t  m_globalWorkSize[2];
        std::vector<OCLKernelArg*> m_argsRGBtoRGBbuffer;
        std::vector<OCLKernelArg*> m_argsRGBbuffertoRGBA;
        OCLKernelArgSharedSurface m_surfRGB;
        OCLKernelArgSurface m_surfRGBbuffer;

        OCLKernelArgInt m_cols;
        OCLKernelArgInt m_channelSz;

    };

    class OCLFilterStore {
    public:
        OCLFilterStore(OCLEnv* env);
        virtual ~OCLFilterStore();
        bool Create(const std::string& programSource);
        OCLKernel* CreateKernel(const std::string& name);
    private:
        OCLProgram m_program;
        OCLEnv* m_env;
    };

    OCLFilterStore* CreateFilterStore(OCLEnv* env, const std::string& oclFile);
