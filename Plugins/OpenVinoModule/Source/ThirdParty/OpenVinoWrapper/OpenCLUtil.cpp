#include "OpenCLUtil.h"

#include <thread>
#include <iostream>
#include <fstream>

#define MAX_PLATFORMS       32
#define MAX_STRING_SIZE     1024

#define INIT_CL_EXT_FUNC(x)    x = (x ## _fn)clGetExtensionFunctionAddress(#x);
#define SAFE_OCL_FREE(P, FREE_FUNC)  { if (P) { FREE_FUNC(P); P = NULL; } }
#define EXT_INIT(_p, _name) _name = (_name##_fn) clGetExtensionFunctionAddressForPlatform((_p), #_name); res &= (_name != NULL);

#define DEBUG_FALG false

    // OCLProgram methods
    OCLProgram::OCLProgram(OCLEnv* env) : m_program(nullptr), m_env(env) {}

    OCLProgram::~OCLProgram() {}

    bool OCLProgram::Build(const std::string& buildSource, const std::string& buildOptions) {
        std::cout << "OCLProgram: Reading and compiling OCL kernels" << std::endl;

        cl_int error = CL_SUCCESS;
        const char* buildSrcPtr = buildSource.c_str();
        m_program = clCreateProgramWithSource(m_env->GetContext(), 1, &(buildSrcPtr), NULL, &error);
        if (error) {
            std::cout << "OpenCLFilter: clCreateProgramWithSource failed. Error code: " << error << std::endl;
            return error;
        }

        // Build OCL kernel
        cl_device_id pDev = m_env->GetDevice();
        error = clBuildProgram(m_program, 1, &(pDev), buildOptions.c_str(), NULL, NULL);
        if (error == CL_BUILD_PROGRAM_FAILURE)
        {
            size_t buildLogSize = 0;
            cl_int logStatus = clGetProgramBuildInfo(m_program, m_env->GetDevice(), CL_PROGRAM_BUILD_LOG, 0, NULL, &buildLogSize);
            std::vector<char> buildLog(buildLogSize + 1);
            logStatus = clGetProgramBuildInfo(m_program, m_env->GetDevice(), CL_PROGRAM_BUILD_LOG, buildLogSize, &buildLog[0], NULL);
            std::cerr << std::string(buildLog.begin(), buildLog.end()).c_str() << std::endl;
            return false;
        }

        return (error == CL_SUCCESS);
    }

    // OCLKernel methods
    OCLKernel::OCLKernel(OCLEnv* env) : m_env(env) {
    }
    OCLKernel::~OCLKernel() {}

    // OCLKernelArg methods
    OCLKernelArg::OCLKernelArg() : m_idx(0) {}
    OCLKernelArg::~OCLKernelArg() {}

    // OCLEnv methods
    OCLEnv::OCLEnv() :
        m_d3d11device(nullptr),
        m_cldevice(nullptr),
        m_clplatform(nullptr),
        m_clcontext(nullptr),
        m_clqueue(nullptr),
        m_type(OCL_GPU_UNDEFINED) {}

    OCLEnv::~OCLEnv() {
        SAFE_OCL_FREE(m_clcontext, clReleaseContext);
        SAFE_OCL_FREE(m_clqueue, clReleaseCommandQueue);
    }

    bool OCLEnv::Init(cl_platform_id clplatform) {
        m_clplatform = clplatform;
        bool res = true;

        EXT_INIT(m_clplatform, clGetDeviceIDsFromD3D11KHR);
        EXT_INIT(m_clplatform, clCreateFromD3D11Texture2DKHR);
        EXT_INIT(m_clplatform, clEnqueueAcquireD3D11ObjectsKHR);
        EXT_INIT(m_clplatform, clEnqueueReleaseD3D11ObjectsKHR);

        return res;
    }

    cl_command_queue OCLEnv::GetCommandQueue() {
        return m_clqueue;
    }

    bool OCLEnv::SetD3DDevice(ID3D11Device* device) {
        cl_int error = CL_SUCCESS;

        m_d3d11device = device;

        cl_uint numDevices = 0;
        error = clGetDeviceIDsFromD3D11KHR(m_clplatform,
            CL_D3D11_DEVICE_KHR,
            m_d3d11device,
            CL_PREFERRED_DEVICES_FOR_D3D11_KHR,
            1,
            &m_cldevice,
            &numDevices);

        if (error != CL_SUCCESS) {
            std::cerr << "OCLEnv: clGetDeviceIDsFromD3D11KHR failed. Error code: " << error << std::endl;
            return false;
        }

        // Create context
        const cl_context_properties props[] = { CL_CONTEXT_D3D11_DEVICE_KHR, (cl_context_properties)m_d3d11device,
            CL_CONTEXT_INTEROP_USER_SYNC, CL_FALSE, NULL };
        m_clcontext = clCreateContext(props, 1, &m_cldevice, NULL, NULL, &error);
        if (error != CL_SUCCESS) {
            std::cerr << "OCLEnv: clCreateContext failed. Error code: " << error << std::endl;
            return false;
        }

        // Create command queue
        m_clqueue = clCreateCommandQueue(m_clcontext, m_cldevice, 0, &error);
        if (!m_clqueue) {
            std::cerr << "OCLEnv: clCreateCommandQueue failed. Error code: " << error << std::endl;
            return false;
        }
        printf("Create command queue: %p\n", m_clqueue);

        // Check device type
        cl_bool hostUnifiedMemory;
        clGetDeviceInfo(m_cldevice, CL_DEVICE_HOST_UNIFIED_MEMORY, sizeof(hostUnifiedMemory), &hostUnifiedMemory, nullptr);
        m_type = hostUnifiedMemory ? OCL_GPU_INTEGRATED : OCL_GPU_DISCRETE;

        std::cout << "OCLEnv: OCL device initiated. OCL device type: "
            << ((m_type == OCL_GPU_INTEGRATED) ? "OCL_GPU_INTEGRATED" : "OCL_GPU_DISCRETE") << std::endl;

        return (error == CL_SUCCESS);
    }

    cl_mem OCLEnv::CreateSharedSurface(ID3D11Texture2D* surf, int nView, bool bIsReadOnly) {
        auto it = m_sharedSurfs.find(SurfaceKey(surf, nView));
        if (it != m_sharedSurfs.end()) {
            return it->second;
        }

        cl_int error = CL_SUCCESS;
        cl_mem mem = clCreateFromD3D11Texture2DKHR(m_clcontext, bIsReadOnly ? CL_MEM_READ_ONLY : CL_MEM_READ_WRITE, surf, nView, &error);
        if (error != CL_SUCCESS) {
            std::cerr << "clCreateFromD3D11Texture2DKHR failed. Error code: " << error << std::endl;
            return nullptr;
        }
        m_sharedSurfs[SurfaceKey(surf, nView)] = mem;

        return mem;
    }

    bool OCLEnv::EnqueueAcquireSurfaces(cl_mem* surfaces, int nSurfaces, bool flushAndFinish)
    {
        std::lock_guard<std::mutex> lock(m_sharedSurfMutex);

        cl_command_queue cmdQueue = GetCommandQueue();
        if (!cmdQueue) {
            return false;
        }
        cl_int error = clEnqueueAcquireD3D11ObjectsKHR(cmdQueue, nSurfaces, surfaces, 0, NULL, NULL);
        if (error) {
            printf("clEnqueueAcquireD3D11ObjectsKHR (cmdQueue = %p) failed. Error code: %d\n", cmdQueue, error);
            return false;
        }

        if (flushAndFinish) {
            // flush & finish the command queue
            error = clFlush(cmdQueue);
            if (error) {
                std::cerr << "clFlush failed. Error code: " << error << std::endl;
                return false;
            }
            error = clFinish(cmdQueue);
            if (error) {
                std::cerr << "clFinish failed. Error code: " << error << std::endl;
                return false;
            }
        }

        return true;
    }

    bool OCLEnv::EnqueueReleaseSurfaces(cl_mem* surfaces, int nSurfaces, bool flushAndFinish)
    {
        std::lock_guard<std::mutex> lock(m_sharedSurfMutex);

        cl_command_queue cmdQueue = GetCommandQueue();
        if (!cmdQueue) {
            return false;
        }
        cl_int error = clEnqueueReleaseD3D11ObjectsKHR(cmdQueue, nSurfaces, surfaces, 0, NULL, NULL);
        if (error) {
            std::cerr << "clEnqueueReleaseD3D11ObjectsKHR failed. Error code: " << error << std::endl;
            return false;
        }

        if (flushAndFinish) {
            // flush & finish the command queue
            error = clFlush(cmdQueue);
            if (error) {
                std::cerr << "clFlush failed. Error code: " << error << std::endl;
                return false;
            }
            error = clFinish(cmdQueue);
            if (error) {
                std::cerr << "clFinish failed. Error code: " << error << std::endl;
                return false;
            }
        }
        return true;
    }

    bool OCLEnv::EnqueueAcquireSurfaces(cl_command_queue command_queue,
        cl_uint num_objects,
        const cl_mem* mem_objects,
        cl_uint num_events_in_wait_list,
        const cl_event* event_wait_list,
        cl_event* event) {
        cl_int error = clEnqueueAcquireD3D11ObjectsKHR(command_queue, num_objects, mem_objects, num_events_in_wait_list, event_wait_list, event);

        if (error) {
            std::cerr << "clEnqueueAcquireD3D11ObjectsKHR failed. Error code: " << error << std::endl;
            return false;
        }

        return true;
    }

    bool OCLEnv::EnqueueReleaseSurfaces(cl_command_queue command_queue,
        cl_uint num_objects,
        const cl_mem* mem_objects,
        cl_uint num_events_in_wait_list,
        const cl_event* event_wait_list,
        cl_event* event) {
        cl_int error = clEnqueueReleaseD3D11ObjectsKHR(command_queue, num_objects, mem_objects, num_events_in_wait_list, event_wait_list, event);

        if (error) {
            std::cerr << "clEnqueueReleaseD3D11ObjectsKHR failed. Error code: " << error << std::endl;
            return false;
        }

        return true;
    }

    OCL::OCL() {}

    bool OCL::Init()
    {
        cl_int error = CL_SUCCESS;

        // Determine the number of installed OpenCL platforms
        cl_uint num_platforms = 0;
        error = clGetPlatformIDs(0, NULL, &num_platforms);
        if (error)
        {
            std::cerr << "OpenCL: Couldn't get platform IDs. \
                        Make sure your platform \
                        supports OpenCL and can find a proper library. Error Code:" << error << std::endl;
            return false;
        }

        // Get all of the handles to the installed OpenCL platforms
        std::vector<cl_platform_id> platforms(num_platforms);
        error = clGetPlatformIDs(num_platforms, &platforms[0], &num_platforms);
        if (error) {
            std::cerr << "OpenCL: Failed to get OCL platform IDs. Error Code: " << error << std::endl;
            return false;
        }

        // Find the platform handle for the installed Gen driver
        const size_t max_string_size = 1024;
        char platform[max_string_size];
        cl_device_id device_ids[2] = { 0 };
        for (unsigned int platform_index = 0; platform_index < num_platforms; platform_index++)
        {
            error = clGetPlatformInfo(platforms[platform_index], CL_PLATFORM_NAME, max_string_size, platform, NULL);
            if (error) {
                std::cerr << "OpenCL: Failed to get platform info. Error Code: " << error << std::endl;
                return false;
            }

            // Choose only GPU devices
            if (clGetDeviceIDs(platforms[platform_index], CL_DEVICE_TYPE_GPU,
                sizeof(device_ids) / sizeof(device_ids[0]), device_ids, 0) != CL_SUCCESS)
                continue;

            if (strstr(platform, "Intel")) // Use only Intel platfroms
            {
                std::cout << "OpenCL platform \"" << platform << "\" is used" << std::endl;
                std::shared_ptr<OCLEnv> env(new OCLEnv);
                if (env->Init(platforms[platform_index])) {
                    m_envs.push_back(env);
                }
                else {
                    std::cerr << "Faild to initialize OCL sharing extenstions" << std::endl;
                    return false;
                }
            }
        }
        if (0 == m_envs.size())
        {
            std::cerr << "OpenCLFilter: Didn't find an Intel platform!" << std::endl;
            return false;
        }

        return true;
    }

    std::shared_ptr<OCLEnv> OCL::GetEnv(ID3D11Device* dev) {
        if (!dev) {
            std::cerr << "D3D11Device pointer is invalid." << std::endl;
            return nullptr;
        }

        // Try to find compatible OCL environment
        for (int i = 0; i < m_envs.size(); i++) {
            if (m_envs[i]->SetD3DDevice(dev)) {
                return m_envs[i];
            }
        }
        std::cerr << "No matching OpenCL devices was found for D3D11 device." << std::endl;
        return nullptr;
    }

    // OCLKernelArgSharedSurface methods
    OCLKernelArgSurface::OCLKernelArgSurface() : m_hdl(nullptr) {}
    OCLKernelArgSurface::~OCLKernelArgSurface() {}

    bool OCLKernelArgSurface::Set(cl_kernel kernel) {
        cl_int error = CL_SUCCESS;
        error = clSetKernelArg(kernel, m_idx, sizeof(cl_mem), &m_hdl);
        if (error) {
            std::cerr << "clSetKernelArg failed. Error code: " << error << std::endl;
            return false;
        }
        return true;
    }

    // OCLKernelArgInt methods
    OCLKernelArgInt::OCLKernelArgInt() {}
    OCLKernelArgInt::~OCLKernelArgInt() {}
    bool OCLKernelArgInt::Set(cl_kernel kernel) {
        cl_int error = CL_SUCCESS;
        error = clSetKernelArg(kernel, m_idx, sizeof(cl_int), &m_val);
        if (error) {
            std::cerr << "clSetKernelArg failed. Error code: " << error << std::endl;
            return false;
        }
        return true;
    }

    // OCLKernelArgFloat methods
    OCLKernelArgFloat::OCLKernelArgFloat() {}
    OCLKernelArgFloat::~OCLKernelArgFloat() {}
    bool OCLKernelArgFloat::Set(cl_kernel kernel) {
        cl_int error = CL_SUCCESS;
        error = clSetKernelArg(kernel, m_idx, sizeof(cl_float), &m_val);
        if (error) {
            std::cerr << "clSetKernelArg failed. Error code: " << error << std::endl;
            return false;
        }
        return true;
    }

    //SourceConversion methods
    SourceConversion::SourceConversion(OCLEnv* env) : OCLKernel(env) {
        m_surfRGB.SetIdx(0);
        m_argsRGBtoRGBbuffer.push_back(&m_surfRGB);
        m_argsRGBbuffertoRGBA.push_back(&m_surfRGB);

        m_surfRGBbuffer.SetIdx(1); // kernel argument 1
        m_argsRGBtoRGBbuffer.push_back(&m_surfRGBbuffer);
        m_argsRGBbuffertoRGBA.push_back(&m_surfRGBbuffer);

        m_cols.SetIdx(2); // both kernels argument 2
        m_argsRGBtoRGBbuffer.push_back(&m_cols);
        m_argsRGBbuffertoRGBA.push_back(&m_cols);


        m_channelSz.SetIdx(3); // both kernels argument 3
        m_argsRGBtoRGBbuffer.push_back(&m_channelSz);
        m_argsRGBbuffertoRGBA.push_back(&m_channelSz);

    }

    SourceConversion::~SourceConversion() {

    }

    bool SourceConversion::Create(cl_program program) {
        cl_int error = CL_SUCCESS;

        m_kernelRGBtoRGBbuffer = clCreateKernel(program, "convertARGBU8ToRGBint", &error);
        m_kernelRGBbuffertoRGBA = clCreateKernel(program, "convertRGBintToARGB", &error);
        if (error) {
            std::cerr << "OpenCLFilter: clCreateKernel failed. Error code: " << error << std::endl;
            return false;
        }
        return true;
    }
    bool SourceConversion::SetArgumentsRGBtoRGBbuffer(ID3D11Texture2D* in_rgbSurf, cl_mem out_rgbSurf, int cols, int rows) {
        cl_mem in_hdlRGB = m_env->CreateSharedSurface(in_rgbSurf, 0, true); //rgb surface only has one view,default as 0
        if (!in_hdlRGB) {
            return false;
        }

        m_surfRGB.SetHDL(in_hdlRGB);

        m_surfRGBbuffer.SetHDL(out_rgbSurf);


        m_cols.SetVal(cols);
        m_channelSz.SetVal(cols * rows);

        m_globalWorkSize[0] = cols;
        m_globalWorkSize[1] = rows;

        m_RGBToRGBbuffer = true;
        return true;
    }


    bool SourceConversion::SetArgumentsRGBbuffertoRGBA(cl_mem in_rgbSurf, ID3D11Texture2D* out_rgbSurf, int cols, int rows) {
#if DEBUG_FALG
        cl_command_queue cmdQueue = m_env->GetCommandQueue();
        if (!cmdQueue) {
            return false;
        }
        printClVector(in_rgbSurf, cols * rows * 3, 0, cmdQueue);
#endif
        cl_mem out_hdlRGB = m_env->CreateSharedSurface(out_rgbSurf, 0, false); //rgb surface only has one view,default as 0
        if (!out_hdlRGB) {
            return false;
        }

        m_surfRGB.SetHDL(out_hdlRGB);

        m_surfRGBbuffer.SetHDL(in_rgbSurf);


        m_cols.SetVal(cols);
        m_channelSz.SetVal(cols * rows);

        m_globalWorkSize[0] = cols;
        m_globalWorkSize[1] = rows;

        m_RGBToRGBbuffer = false;
        return true;
    }

    bool SourceConversion::Run() {
        std::vector<cl_mem> sharedSurfaces;
        std::vector<OCLKernelArg*>& args = m_RGBToRGBbuffer ? m_argsRGBtoRGBbuffer : m_argsRGBbuffertoRGBA;
        cl_kernel& kernel = m_RGBToRGBbuffer ? m_kernelRGBtoRGBbuffer : m_kernelRGBbuffertoRGBA;
        cl_mem input;
        for (int i = 0; i < args.size(); i++) {
            if (!(args[i]->Set(kernel))) {
                return false;
            }

            if (args[i]->Type() == OCLKernelArg::OCL_KERNEL_ARG_SHARED_SURFACE) {
                cl_mem hdl = dynamic_cast<OCLKernelArgSharedSurface*>(args[i])->GetHDL();
                sharedSurfaces.push_back(hdl);
            }

            if (args[i]->Type() == OCLKernelArg::OCL_KERNEL_ARG_SURFACE)
            {
                input = dynamic_cast<OCLKernelArgSurface*>(args[i])->GetHDL();
            }
        }

        cl_int error = CL_SUCCESS;
        cl_command_queue cmdQueue = m_env->GetCommandQueue();
        if (!cmdQueue) {
            return false;
        }

        if (!m_env->EnqueueAcquireSurfaces(&sharedSurfaces[0], sharedSurfaces.size(), false)) {
            return false;
        }

        error = clEnqueueNDRangeKernel(cmdQueue, kernel, 2, NULL, m_globalWorkSize, NULL, 0, NULL, NULL);
        if (error) {
            std::cerr << "clEnqueueNDRangeKernel failed. Error code: " << error << std::endl;
            return false;
        }

        // test
        /*uint8_t data[100] = {1,2,3,4};
        cl_int err_flag;
        cl_mem t = clCreateBuffer(m_env->GetContext(), CL_MEM_READ_WRITE| CL_MEM_USE_HOST_PTR, 100, data, &err_flag);
        if (err_flag)
        {
            std::cout << "erro" << std::endl;
        }*/
#if DEBUG_FLAG
        if (m_RGBToRGBbuffer) { //image
            printClVector(sharedSurfaces[0], 3060 * 1204 * 4,  1, cmdQueue);
         }
#endif


        if (!m_env->EnqueueReleaseSurfaces(&sharedSurfaces[0], sharedSurfaces.size(), false)) {
            return false;
        }

        // flush & finish the command queue
        error = clFlush(cmdQueue);
        if (error) {
            std::cerr << "clFlush failed. Error code: " << error << std::endl;
            return false;
        }
        error = clFinish(cmdQueue);
        if (error) {
            std::cerr << "clFinish failed. Error code: " << error << std::endl;
            return false;
        }

        return (error == CL_SUCCESS);
    }

    void SourceConversion::printClVector(cl_mem& clVector, int length, cl_command_queue& commands, int datatype, int printrowlen)
    {
        uint8_t* tmp = new uint8_t[length];
        int err;
        if (datatype == 0) // buffer
        {
            err = clEnqueueReadBuffer(commands, clVector, CL_TRUE, 0, sizeof(uint8_t) * length, tmp, 0, NULL, NULL);
        }
        if (datatype == 1) // image
        {
            size_t origin[3] = { 0,0,0 };
            size_t region[3] = { 3060,1204,1 };
            err = clEnqueueReadImage(commands, clVector, CL_TRUE, origin, region, 0, 0, tmp, 0, NULL, NULL);
        }
      
        
        if (err != CL_SUCCESS)
        {
            printf("Error: Failed to read output array! %d\n", err);
            exit(1);
        }
        if (printrowlen < 0)	// print all as one line
        {
            std::ofstream myfile;
            myfile.open("log.txt", std::ios::out | std::ios::binary);
            for (size_t i = 0; i < length; i++) {
                //std::cout << (int)data[i] << " ";
                myfile << tmp[i];
                //myfile << (int)data[i] << " ";
                //if (i > 1280*5) break;
            }
            myfile.close();
        }
        else				// print rows of "printrowlen" length
        {
            for (int k = 0; k < length / printrowlen; k++)
            {
                for (int j = 0; j < printrowlen; j++)
                {
                    std::cout << tmp[k * printrowlen + j] << " ";
                }
                std::cout << std::endl;
            }
            std::cout << std::endl;
        }
        delete[] tmp;
    }

    //OCLFilterStore methods
    OCLFilterStore::OCLFilterStore(OCLEnv* env) : m_env(env), m_program(env) {}
    OCLFilterStore::~OCLFilterStore() {}

    bool OCLFilterStore::Create(const std::string& programSource) {
        std::string buildOptions = "-I. -Werror -cl-fast-relaxed-math";


        if (!m_program.Build(programSource, buildOptions)) {
            return false;
        }
        return true;
    }

    OCLKernel* OCLFilterStore::CreateKernel(const std::string& name) {
        OCLKernel* kernel = nullptr;
        if (name == "srcConversion") {
            kernel = new SourceConversion(m_env);
            if (!kernel->Create(m_program.GetHDL())) {
                delete kernel;
                kernel = nullptr;
            }
        }
        return kernel;
    }

    static std::string getPathToExe()
    {
        const size_t module_length = 1024;
        char module_name[module_length];

#if defined(_WIN32) || defined(_WIN64)
        GetModuleFileNameA(0, module_name, module_length);
#else
        char id[module_length];
        sprintf(id, "/proc/%d/exe", getpid());
        ssize_t count = readlink(id, module_name, module_length - 1);
        if (count == -1)
            return std::string("");
        module_name[count] = '\0';
#endif

        std::string exePath(module_name);
        return exePath.substr(0, exePath.find_last_of("\\/") + 1);
    }

    static std::string getPathToHandle()
    {
        char path[MAX_PATH];
        HMODULE hm = NULL;

        if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)"OpenVinoWrapper.dll", &hm) == 0)
        {
            int ret = GetLastError();
            fprintf(stderr, "GetModuleHandle failed, error = %d\n", ret);
            // Return or however you want to handle an error.
        }
        if (GetModuleFileName(hm, path, sizeof(path)) == 0)
        {
            int ret = GetLastError();
            fprintf(stderr, "GetModuleFileName failed, error = %d\n", ret);
            // Return or however you want to handle an error.
        }
        std::string dllPath(path);
        return dllPath.substr(0, dllPath.find_last_of("\\/") + 1);

    }

    static std::string readFile(const char* filename)
    {
        std::cout << "Info: try to open file (" << filename << ") in the current directory" << std::endl;
        std::ifstream input(filename, std::ios::in | std::ios::binary);

        if (!input.good())
        {
            // look in folder with executable
            input.clear();

            std::string module_name = getPathToHandle() + std::string(filename);

            std::cout << "Info: try to open file: " << module_name.c_str() << std::endl;
            input.open(module_name.c_str(), std::ios::binary);
        }

        if (!input)
            throw std::logic_error((std::string("Error_opening_file_\"") + std::string(filename) + std::string("\"")).c_str());

        input.seekg(0, std::ios::end);
        std::vector<char> program_source(static_cast<int>(input.tellg()));
        input.seekg(0);

        input.read(&program_source[0], program_source.size());

        return std::string(program_source.begin(), program_source.end());
    }

    OCLFilterStore* CreateFilterStore(OCLEnv* env, const std::string& oclFile) {
        OCLFilterStore* filterStore = new OCLFilterStore(env);

        std::string buffer = readFile(oclFile.c_str());
        if (!filterStore->Create(buffer)) {
            return nullptr;
        }
        return filterStore;
    }