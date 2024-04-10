// GetProcessesCPP.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <iostream>
#include <Windows.h>
#include <string>
#include <stdio.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <map>


static int numProcessors;
static std::map<std::wstring, std::unordered_set<DWORD>>processMap; //executable and # of subprocesses
static std::map<std::wstring, DWORD>parentMap;
static HANDLE GlobalHSnap;

    void splitDirectory(std::wstring dir)
    {
        int i = 0;
        for (int j = 0; j < dir.length(); j++)
        {
            if (dir[j] == L'\\')
            {
                std::wcout << dir.substr(i, j - i) << " | ";
                i = ++j;
            }
        }
        std::wcout << dir.substr(i, dir.length() - i) << std::endl;
    }

    bool CheckPID(DWORD d, HANDLE hSnapCopy)
    {
        PROCESSENTRY32 pr32;
        pr32.dwSize = sizeof(PROCESSENTRY32);
        DWORD pid = d;
        BOOL found = false;

        if (!Process32First(hSnapCopy, &pr32))
        {
            return false;
        }

        return true;
    }

    void WalkVS(DWORD d, HANDLE hSnapCopy)
    {
        PROCESSENTRY32 pr32;
        pr32.dwSize = sizeof(PROCESSENTRY32);
        HANDLE handle;
        DWORD pid = d;

        if (!Process32First(hSnapCopy, &pr32))
        {
            std::cout << "PROCESS32FIRST FAILED" << std::endl;
            return;
        }

        while (Process32Next(hSnapCopy, &pr32))
        {
            if (pr32.th32ProcessID == pid)
            {
                std::wstring ws;
                handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
                if (handle != NULL)
                {
                    wchar_t* path = new wchar_t[MAX_PATH];
                    DWORD size = MAX_PATH;
                    BOOL criticalHit;
                    if (QueryFullProcessImageNameW(handle, 0, path, &size) && IsProcessCritical(handle, &criticalHit))
                    {
                        std::wstring ws = path;
                        processMap[ws].insert(pid);
                        //splitDirectory(ws);
                    }
                    CloseHandle(handle);
                    WalkVS(pr32.th32ParentProcessID, hSnapCopy);
                }
                else
                {
                    parentMap[ws] = pid;
                }
            }
        }
    }

    std::wstring ProcessIDName(HANDLE handle, DWORD pid)
    {
        std::wstring name;
        DWORD buffSize = MAX_PATH;
        wchar_t* path = new wchar_t[MAX_PATH];
        LPWSTR windowName;

        if (QueryFullProcessImageNameW(handle, 0, path, &buffSize))
        {
            name = path;
            //if (processMap.find(name) != processMap.end())
                //processMap[name].push_back(pid);
            //std::wcout << name << ": " << pid << '\n';
        }

        return name;
    }

    BOOL CALLBACK enumWindowsCB(HWND hwnd, LPARAM lParam)
    {
        if (!IsWindowVisible(hwnd))
            return TRUE;

        int length = GetWindowTextLength(hwnd) + 1;
        wchar_t* buffer = new wchar_t[length];
        GetWindowTextW(hwnd, buffer, length);
        DWORD id; GetWindowThreadProcessId(hwnd, &id);

        wchar_t* path = new wchar_t[MAX_PATH];
        DWORD size = MAX_PATH;
        HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, id);

        if (QueryFullProcessImageNameW(hProc, 0, path, &size))
        {
            std::wstring ws = path;
            if (processMap.find(ws) == processMap.end())
                processMap[ws] = std::unordered_set<DWORD>();

        }

        //std::unordered_set <std::string>& windows = *reinterpret_cast<std::unordered_set<std::string>*>(lParam);
        return TRUE;
    }

    void GetCPUUsage(DWORD pid)
    {
        HANDLE hProcess;
        PROCESS_MEMORY_COUNTERS pmc;

        hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc)))
        {
            std::cout << "CPU USAGE: " << '\n';
            std::wcout << pmc.QuotaPeakPagedPoolUsage << '\n';
            std::wcout << pmc.QuotaPeakNonPagedPoolUsage << '\n';
        }

        CloseHandle(hProcess);
    }

    BOOL ResetProcessWalker(int count, HANDLE hSnap)
    {
        PROCESSENTRY32 p;
        p.dwSize = sizeof(PROCESSENTRY32);
        if (!Process32First(hSnap, &p))
        {
            std::cout << "RESET FAILURE" << '\n';
            return false;
        }
        while (count > 1)
        {
            if (Process32Next(hSnap, &p))
            {
                HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, p.th32ProcessID);
                if (hProcess)
                    count--;
            }
        }

        return true;
    }

    int main()
    {
        SYSTEM_INFO sysInfo;
        HANDLE hProcsSnap;
        HANDLE sysModulesScreenshot;

        GetSystemInfo(&sysInfo);
        numProcessors = sysInfo.dwNumberOfProcessors;

        hProcsSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        GlobalHSnap = hProcsSnap;
        UINT32 critProcNum = 0;

        if (hProcsSnap != INVALID_HANDLE_VALUE)
        {
            PROCESSENTRY32 prEntry;
            prEntry.dwSize = sizeof(PROCESSENTRY32);

            if (!Process32First(hProcsSnap, &prEntry))
            {
                CloseHandle(hProcsSnap);
                return 0;
            }

            EnumWindows(enumWindowsCB, reinterpret_cast<LPARAM>(&processMap));
            HANDLE hProcess;
            while (Process32Next(hProcsSnap, &prEntry))
            {
                hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, prEntry.th32ProcessID);
                BOOL critProc;
                if (hProcess)
                {
                    critProcNum++;
                    if (IsProcessCritical(hProcess, &critProc))
                    {
                        //std::wcout << ProcessIDName(hProcess, prEntry.th32ProcessID) << '\n';
                        WalkVS(prEntry.th32ProcessID, hProcsSnap);
                        if (!ResetProcessWalker(critProcNum, hProcsSnap))
                            return 0;
                    }
                    //std::cout << critProcNum  << " -------------------------" << std::endl;
                }
            }


            CloseHandle(hProcsSnap);
            //std::cout << processMap.size() << " " << parentMap.size() << '\n';
            int d = processMap.size();
            for (auto& [i, j] : processMap)
            {
                std::wcout << i << " | ";
                for (auto& k : j)
                {
                    std::wcout << k << " ";
                    d++;
                }
                std::cout << std::endl;
            }
            std::cout << d;
            return 0;
        }
    }
// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
