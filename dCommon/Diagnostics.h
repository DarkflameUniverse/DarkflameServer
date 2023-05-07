#pragma once

#include <string>

class Diagnostics
{
public:
	static void Initialize();

	static void SetProcessName(const std::string& name);

	static void SetProcessFileName(const std::string& name);

	static void SetOutDirectory(const std::string& path);

	static void SetProduceMemoryDump(bool value);

	static const std::string& GetProcessName();

	static const std::string& GetProcessFileName();

	static const std::string& GetOutDirectory();

	static bool GetProduceMemoryDump();

private:
	static std::string m_ProcessName;
	static std::string m_ProcessFileName;
	static std::string m_OutDirectory;
	static bool m_ProduceMemoryDump;
};
