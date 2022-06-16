/*
"Copyright 2019 Intel Corporation.

The source code, information and material ("Material") contained herein is owned by Intel Corporation or its 
suppliers or licensors, and title to such Material remains with Intel Corporation or its suppliers or licensors. 
The Material contains proprietary information of Intel or its suppliers and licensors. The Material is 
protected by worldwide copyright laws and treaty provisions. No part of the Material may be used, copied, 
reproduced, modified, published, uploaded, posted, transmitted, distributed or disclosed in any way without Intel's 
prior express written permission. No license under any patent, copyright or other intellectual property rights in 
the Material is granted to or conferred upon you, either expressly, by implication, inducement, estoppel or otherwise. 
Any license under such intellectual property rights must be express and approved by Intel in writing.


Include supplier trademarks or logos as supplier requires Intel to use, preceded by an asterisk. An asterisked footnote 
can be added as follows: *Third Party trademarks are the property of their respective owners.

Unless otherwise agreed by Intel in writing, you may not remove or alter this notice or any other notice 
embedded in Materials by Intel or Intel's suppliers or licensors in any way."
*/


// Fill out your copyright notice in the Description page of Project Settings.
using System;
using System.IO;
using System.Collections.Generic;
using UnrealBuildTool;

public class OpenVinoWrapper : ModuleRules
{
	public OpenVinoWrapper(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
            // Add the import library
            PublicLibraryPaths.Add(Path.Combine(ModuleDirectory, "bin"));
			PublicAdditionalLibraries.Add("OpenVinoWrapper.lib");

            CopyOpenVinoDll();

            string openVinoDir = Path.Combine(
                Path.GetFullPath(PluginDirectory), 
                "Source", 
                "ThirdParty", 
                "OpenVinoWrapper");

            CopyToBinariesAddRuntimeDependancies(Path.Combine(openVinoDir, "opencv/bin/dll_release"));
            CopyToBinariesAddRuntimeDependancies(Path.Combine(openVinoDir, "openvino/bin/intel64/dll_release"));

            CopyToBinariesAddRuntimeDependancies(openVinoDir, "opencv/bin/dll_release");
            CopyToBinariesAddRuntimeDependancies(openVinoDir, "openvino/bin/intel64/dll_release");
        }
        else
        {
            throw new PlatformNotSupportedException("Platform not supported: " + Target.Platform.ToString());
        }
	}

    void CopyOpenVinoDll()
    {
        var openVinoDllPath = Path.Combine(ModuleDirectory, "bin", "OpenVinoWrapper.dll");
        if (!File.Exists(openVinoDllPath))
            throw new FileNotFoundException(openVinoDllPath);

        {
            var projectPath = Directory.GetParent(Path.GetFullPath(PluginDirectory)).Parent.FullName.ToString();
            var binariesDir = Path.Combine(projectPath, "Binaries", Target.Platform.ToString());
            if (!Directory.Exists(binariesDir))
                Directory.CreateDirectory(binariesDir);

            var pluginDestination = Path.Combine(binariesDir, "OpenVinoWrapper.dll");

            Console.WriteLine("Source:      {0}", openVinoDllPath);
            Console.WriteLine("Destination: {0}", pluginDestination);

            if (!File.Exists(pluginDestination) ||
                (File.GetLastWriteTime(openVinoDllPath) > File.GetLastWriteTime(pluginDestination)))
                File.Copy(openVinoDllPath, pluginDestination, true);
            RuntimeDependencies.Add(pluginDestination);
        }
        {
            var pluginPath = Path.GetFullPath(PluginDirectory);
            var binariesDir = Path.Combine(pluginPath, "Binaries", Target.Platform.ToString());
            var pluginDestination = Path.Combine(binariesDir, "OpenVinoWrapper.dll");
            if (!Directory.Exists(binariesDir))
                Directory.CreateDirectory(binariesDir);

            Console.WriteLine("Source:      {0}", openVinoDllPath);
            Console.WriteLine("Destination: {0}", pluginDestination);

            if (!File.Exists(pluginDestination) ||
                (File.GetLastWriteTime(openVinoDllPath) > File.GetLastWriteTime(pluginDestination)))
                File.Copy(openVinoDllPath, pluginDestination, true);
            RuntimeDependencies.Add(pluginDestination);
        }
    }

    void CopyToBinariesAddRuntimeDependancies(string baseDirPath, string subDir)
    {
        var sourceDirPath = Path.Combine(baseDirPath, subDir);
        var pluginPath = Path.GetFullPath(PluginDirectory);
        var pluginBinariesDir = Path.Combine(pluginPath, "Binaries", Target.Platform.ToString(), subDir);

        if (Directory.Exists(sourceDirPath))
        {
            Console.WriteLine("CopyDir: {0} -> {1}", sourceDirPath, pluginBinariesDir);

            DirectoryCopyRecursive(sourceDirPath, pluginBinariesDir);
        }
    }

    void CopyToBinariesAddRuntimeDependancies(string sourceDirPath)
    {
        var projectPath = Directory.GetParent(Path.GetFullPath(PluginDirectory)).Parent.FullName.ToString();
        var binariesDir = Path.Combine(projectPath, "Binaries", Target.Platform.ToString());

        if (Directory.Exists(sourceDirPath))
        {
            Console.WriteLine("CopyDir: {0} -> {1}", sourceDirPath, binariesDir);

            DirectoryCopyRecursive(sourceDirPath, binariesDir);
        }
    }

    List<string> fileList = new List<string>();
    void DirectoryCopyRecursive(string sourceDir, string destDir)
    {
        var sourceDirectoryInfo = new DirectoryInfo(sourceDir);

        if (!sourceDirectoryInfo.Exists)
            throw new DirectoryNotFoundException("Source directory not found: " + sourceDirectoryInfo.FullName);

        if (!Directory.Exists(destDir))
            Directory.CreateDirectory(destDir);

        var files = sourceDirectoryInfo.GetFiles();
        foreach (var fi in files)
        {
            var dfi = new FileInfo(Path.Combine(destDir, fi.Name));
            Console.WriteLine(dfi.FullName);

            if (!dfi.Exists || dfi.LastWriteTime < fi.LastWriteTime || dfi.Length != fi.Length)
            {
                fi.CopyTo(dfi.FullName, true);
            }

            if (!fileList.Contains(fi.Name))
            {
                RuntimeDependencies.Add(dfi.FullName);
                fileList.Add(fi.Name);

                if (dfi.Extension.Equals(".dll", StringComparison.OrdinalIgnoreCase))
                    PublicDelayLoadDLLs.Add(dfi.Name);
            }
        }

        var directories = sourceDirectoryInfo.GetDirectories();
        foreach (var di in directories)
            DirectoryCopyRecursive(di.FullName, Path.Combine(destDir, di.Name));
    }
}
