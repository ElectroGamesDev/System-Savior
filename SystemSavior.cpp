#include <iostream>
#include <filesystem>
#include <windows.h>
#include <shellapi.h>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <array>
#include <memory>
#include <cstdio>
#include <thread>
#include <fstream>
#include <regex>
#include <iostream>
#include <sstream>
#include <chrono>
#include <ctime>

#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define RESET   "\033[0m"

int filesRemoved = 0;
std::uintmax_t storageCleared = 0;
int corruptedSystemFilesFixed = 0;
int corruptedSystemFilesFailed = 0;
int systemFilesScanned = 0;
std::vector<std::filesystem::path> foldersToClear;

bool IsAdmin()
{
    BOOL is_admin = FALSE;
    SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
    PSID admin_group;
    if (AllocateAndInitializeSid(&nt_authority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &admin_group))
    {
        if (!CheckTokenMembership(NULL, admin_group, &is_admin))
        {
            is_admin = FALSE;
        }
        FreeSid(admin_group);
    }
    return is_admin != 0;
}

std::string ExtractNumbers(std::string str)
{
    std::string numberString;
    for (char& ch : str)
    {
        if (std::isdigit(ch)) {
            numberString += ch;
        }
        else if (!numberString.empty()) {
            break;
        }
    }
    return numberString;
}

void FindTempFolders(std::filesystem::path path)
{
    try
    {
        for (auto& entry : std::filesystem::directory_iterator(path))
        {
            try
            {
                if (entry.is_directory())
                {
                    if (entry.path().filename().string() == "Documents" || entry.path().filename().string() == "Desktop") continue;
                    FindTempFolders(entry.path());
                    if (entry.path().filename().string() == "Temp")
                    {
                        //std::cout << "Found Temp directory: " << entry.path() << "\n";
                        foldersToClear.push_back(entry.path());
                    }
                }
            }
            catch (const std::filesystem::filesystem_error& ex) {
                //std::cerr << "Filesystem error: " << ex.what() << "\n";
            }
        }
    }
    catch (const std::filesystem::filesystem_error& ex) {}
}

void ClearUnnecessaryFiles()
{
    //std::cout << "Cleaning Up The System" << std::endl;

    //std::filesystem::path temp_dir = std::filesystem::temp_directory_path();

    std::cout << "\nRemoving Clutter " << YELLOW "[0%]" << RESET << std::flush;

    int recycleBinFileCount = 0;
    SHQUERYRBINFO recycleBinInfo;
    recycleBinInfo.cbSize = sizeof(recycleBinInfo);
    if (SUCCEEDED(SHQueryRecycleBin(NULL, &recycleBinInfo))) recycleBinFileCount = recycleBinInfo.i64NumItems;

    //std::int64_t file_count = static_cast<std::int64_t>(std::distance(std::filesystem::recursive_directory_iterator(temp_dir), std::filesystem::recursive_directory_iterator{})) + static_cast<std::int64_t>(recycleBinFileCount);
    char* userprofile;
    size_t len;
    errno_t err = _dupenv_s(&userprofile, &len, "USERPROFILE");
    std::filesystem::path root(userprofile);
    foldersToClear.push_back(std::filesystem::path("C:\\Windows\\Temp"));
    FindTempFolders(root);
    int tempFilesFound = 0;
    for (const std::filesystem::path& path : foldersToClear)
        tempFilesFound += static_cast<std::int64_t>(std::distance(std::filesystem::recursive_directory_iterator(path), std::filesystem::recursive_directory_iterator{}));
    std::int64_t file_count = tempFilesFound + static_cast<std::int64_t>(recycleBinFileCount);
    int progress = 0;

    // Temp Folder
    //if (std::filesystem::exists(temp_dir)) {

    //    for (const auto& entry : std::filesystem::recursive_directory_iterator(temp_dir))
    //    {
    //        if (std::filesystem::is_regular_file(entry))
    //        {
    //            try
    //            {
    //                std::filesystem::remove(entry);
    //                filesRemoved++;
    //                storageCleared += entry.file_size();
    //            }
    //            catch (const std::filesystem::filesystem_error& e) {}
    //        }

    //        ++progress;
    //        int percent_complete = static_cast<int>((static_cast<float>(progress) / file_count) * 100);
    //        std::cout << "\rRemoving Clutter " << YELLOW << "[" << percent_complete << "%]" << RESET << std::flush;
    //    }
    //    for (const auto& entry : std::filesystem::directory_iterator(temp_dir))
    //    {
    //        try 
    //        {
    //            std::filesystem::remove(entry);
    //            filesRemoved++;
    //        }
    //        catch (const std::filesystem::filesystem_error& e) {}
    //    }
    //}

    for (const std::filesystem::path& path : foldersToClear) // Todo: clear folders while clearing files, maybe just loop through the folders to get the file sizes but remove the folders instead of each file
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path))
        {
            if (std::filesystem::is_regular_file(entry))
            {
                try
                {
                    std::filesystem::remove(entry);
                    filesRemoved++;
                    storageCleared += entry.file_size();
                }
                catch (const std::filesystem::filesystem_error& e) {}
            }

            ++progress;
            int percent_complete = static_cast<int>((static_cast<float>(progress) / file_count) * 100);
            std::cout << "\33[2K\rRemoving Clutter " << YELLOW << "[" << percent_complete << "%]" << RESET << std::flush;
        }
        for (const auto& entry : std::filesystem::directory_iterator(path))
        {
            try
            {
                std::filesystem::remove(entry);
                filesRemoved++;
            }
            catch (const std::filesystem::filesystem_error& e) {}
        }
    }

    foldersToClear.clear();
    free(userprofile);

    // Recycle Bin
    recycleBinInfo = { 0 };
    recycleBinInfo.cbSize = sizeof(SHQUERYRBINFO);
    if (SUCCEEDED(SHQueryRecycleBin(NULL, &recycleBinInfo))) storageCleared += recycleBinInfo.i64Size;
    SHEmptyRecycleBin(NULL, NULL, SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
    if (SHQueryRecycleBin(NULL, &recycleBinInfo) == S_OK) {
        int totalCount = recycleBinInfo.i64NumItems;
        int currentCount = 0;

        while (recycleBinInfo.i64NumItems > 0) {
            if (SHQueryRecycleBin(NULL, &recycleBinInfo) == S_OK) {
                ++progress;
                int percent_complete = static_cast<int>((static_cast<float>(progress) / file_count) * 100);
                std::cout << "\33[2K\rRemoving Clutter [" << percent_complete << "%]" << std::flush;
                Sleep(500);
            }
        }
    }

    std::cout << "\33[2K\rRemoving Clutter " << GREEN << "[Complete]" << RESET << std::endl;

}

std::string FormatStorage(std::uintmax_t size_in_bytes)
{
    const char* units[] = { "B", "KB", "MB", "GB", "TB" };
    const std::size_t num_units = sizeof(units) / sizeof(units[0]);

    double size = size_in_bytes;
    std::size_t unit_index = 0;

    while (size >= 1024 && unit_index < num_units - 1) {
        size /= 1024;
        unit_index++;
    }

    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << size << units[unit_index];

    return ss.str();
}

void SfcScan()
{
    if (!IsAdmin())
    {
        std::cout << RED << "\nWARNING: \"Scanning & Repairing Corrupted System Files\" has been skipped due to not having administrator privileges!\n" << RESET << std::endl;
        return;
    }
    //std::cout << "\nRepairing Corrupted Files [0%]" << std::flush;
    std::cout << "\nRepairing Corrupted System Files " << YELLOW "[In Progress]" << RESET << std::flush;

    std::filesystem::path cbsPath("C:\\WINDOWS\\Logs\\CBS");
    try
    {
        std::filesystem::remove(cbsPath / "CBS.log");
    }
    catch(const std::filesystem::filesystem_error& e) {}

    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;
    HANDLE hStdoutRead, hStdoutWrite;
    if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &saAttr, 0)) {
        std::cout << "\33[2K\rReparing Corrupted System Files " << RED << "[Failed]" << RESET << std::endl;;
        return;
    }
    SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStdoutWrite;

    if (!CreateProcessA(nullptr, const_cast<LPSTR>("sfc /scannow"), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
        std::cout << "\33[2K\rReparing Corrupted System Files " << RED << "[Failed]" << RESET << std::endl;
        CloseHandle(hStdoutRead);
        CloseHandle(hStdoutWrite);
        return;
    }

    char buffer[1028];
    DWORD bytesRead;
    bool sfcScanFinished = false;
    int percent = 0;
    while (!sfcScanFinished) {
        if (!ReadFile(hStdoutRead, buffer, sizeof(buffer), &bytesRead, nullptr) || bytesRead == 0) break;
        //std::cout.write(buffer, bytesRead);
        //std::cout.flush();

        std::string output(buffer, bytesRead);
        //std::cout << output << std::endl;
        try { percent = std::stol(output); }
        catch (std::invalid_argument&)
        {
            if (percent > 0)
            {
                sfcScanFinished = true;
                break;
            }
        }

        //std::cout << "\rRepairing Corrupted System Files [" << ExtractNumbers(output) << "%]" << std::flush;

        //if (output.find("Windows Resource Protection") != std::string::npos) sfcScanFinished = true;
        std::cout.flush();
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hStdoutRead);
    CloseHandle(hStdoutWrite);

    std::ifstream file(cbsPath / "CBS.log");
    std::string line;
    std::regex fileScannedPattern(R"(\[SR\] Verifying (\d+) components)");
    std::smatch matches;

    while (std::getline(file, line))
    {
        if (line.find("Cannot repair member") != std::string::npos) corruptedSystemFilesFailed++;
        else if (line.find("Repaired file") != std::string::npos || line.find("Repairing corrupted file") != std::string::npos) corruptedSystemFilesFixed++;
        else if (std::regex_search(line, matches, fileScannedPattern)) systemFilesScanned += std::stoi(matches[1].str());
    }

    std::cout << "\33[2K\rRepairing Corrupted Files " << GREEN << "[Complete]" << RESET << std::endl;
}

void DriveDefragment()
{
    if (!IsAdmin())
    {
        std::cout << RED << "\nWARNING: \"Drive Defragment\" has been skipped due to not having administrator privileges!\n" << RESET << std::endl;
        return;
    }
    //std::cout << "\nRepairing Corrupted Files [0%]" << std::flush;
    std::cout << "\nDrive Defragment " << YELLOW "[In Progress]" << RESET << std::flush;

    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;
    HANDLE hStdoutRead, hStdoutWrite;
    if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &saAttr, 0)) {
        std::cout << "\33[2K\rDrive Defragment " << RED << "[Failed]" << RESET << std::endl;;
        return;
    }
    SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStdoutWrite;

    if (!CreateProcessA(nullptr, const_cast<LPSTR>("defrag C: /A /U"), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
        std::cout << "\33[2K\rDrive Defragment " << RED << "[Failed]" << RESET << std::endl;
        CloseHandle(hStdoutRead);
        CloseHandle(hStdoutWrite);
        return;
    }

    CloseHandle(pi.hThread);

    char buffer[1028];
    DWORD bytesRead;
    bool finished = false;
    int percent = 0;
    std::string entireOutput;
    //while (ReadFile(hStdoutRead, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0)
    //{
    //    std::string output(buffer, bytesRead);
    //    std::cout << output << std::endl;

    //    std::cout << "Drive Defragment [" << ExtractNumbers(output) << "%]" << std::flush;
    //    std::cout.flush();
    //}
    while (!finished) {
        if ((!ReadFile(hStdoutRead, buffer, sizeof(buffer), &bytesRead, nullptr) || bytesRead == 0) && percent > 0)
        {
            finished = true;
            break;
        }
        std::string output(buffer, bytesRead);
        entireOutput += output;
        //std::cout << output << std::endl;
        try
        {
            percent = std::stol(ExtractNumbers(output));
        }
        catch (std::invalid_argument&) {}

        if (entireOutput.find("The operation completed") != std::string::npos ||
            entireOutput.find("You do not need to defragment this volume.") != std::string::npos ||
            entireOutput.find("Defragmentation is not required.") != std::string::npos ||
            entireOutput.find("No files were defragmented.") != std::string::npos ||
            entireOutput.find("The file or directory is not a reparse point.") != std::string::npos ||
            entireOutput.find("The file or directory is not a reparse point, it cannot be defragmented.") != std::string::npos ||
            entireOutput.find("Defragmentation is disabled; run chkdsk or fsutil.") != std::string::npos ||
            entireOutput.find("The last cluster of the file is beyond the end of the volume.") != std::string::npos)
        {
            finished = true;
            break;
        }


        //std::cout << "\33[2K\rDrive Defragment [" << std::to_string(percent) << "%]" << std::flush;
        //std::cout.flush();
    }

    CloseHandle(pi.hProcess);
    CloseHandle(hStdoutRead);
    CloseHandle(hStdoutWrite);

    std::cout << "\33[2K\rDrive Defragment " << GREEN << "[Complete]" << RESET << std::endl;
}

void MiscCommands()
{
    std::cout << "Miscellaneous Repairs " << YELLOW "[In Progress]" << RESET << std::flush;
    std::system("ipconfig /flushdns >nul");
    std::cout << "\33[2K\r" << "Miscellaneous Repairs " << GREEN << "[Complete]" << RESET << std::string("    ") << std::endl;
}

void FinalReport()
{
    int filesRemovedLocation = 31 - std::to_string(filesRemoved).length();
    int storageLocation = 31 - FormatStorage(storageCleared).length();
    std::string corruptedFilesScanned = std::to_string(systemFilesScanned);
    std::string corruptedFilesFixed = std::to_string(corruptedSystemFilesFixed);
    std::string corruptedFilesfailed = std::to_string(corruptedSystemFilesFailed);
    std::string corruptedFilesFound = std::to_string(corruptedSystemFilesFailed + corruptedSystemFilesFixed);
    std::string driveDefragment = "Complete";
    if (!IsAdmin())
    {
        corruptedFilesScanned = "Skipped";
        corruptedFilesFixed = "Skipped";
        corruptedFilesfailed = "Skipped";
        corruptedFilesFound = "Skipped";
        driveDefragment = "Skipped";
    }

    std::cout << BLUE << u8"\n╔══════════════════════════════════════════╗" << std::endl;
    std::cout << BLUE << u8"║                                          ║" << RESET << std::endl;
    std::cout << BLUE << u8"║" << RED << "              Final Report                " << BLUE << u8"║" << RESET << std::endl;
    std::cout << BLUE << u8"║                                          ║" << RESET << std::endl;
    std::cout << BLUE << u8"╟──────────────────────────────────────────╢" << std::endl;
    std::cout << BLUE << u8"║" << RED << "            Clutter Removal               " << BLUE << u8"║" << RESET << std::endl;
    std::cout << BLUE << u8"╟──────────────────────────────────────────╢" << std::endl;
    std::cout << BLUE << u8"║" << GREEN << " Files Removed: " << RESET << filesRemoved << std::setw(filesRemovedLocation) << BLUE << u8"║" << std::endl;
    std::cout << BLUE << u8"║" << GREEN << " Storage Saved: " << RESET << FormatStorage(storageCleared) << std::setw(storageLocation) << BLUE << u8"║" << std::endl;
    std::cout << BLUE << u8"╟──────────────────────────────────────────╢" << std::endl;
    std::cout << BLUE << u8"║" << RED << "      System Files & Registry Repair      " << BLUE << u8"║" << RESET << std::endl;
    std::cout << BLUE << u8"╟──────────────────────────────────────────╢" << std::endl;
    std::cout << BLUE << u8"║" << GREEN << " Files Scanned: " << RESET << corruptedFilesScanned << std::setw(31 - corruptedFilesScanned.length()) << BLUE << u8"║" << std::endl;
    std::cout << BLUE << u8"║" << GREEN << " Corrupted Files Found: " << RESET << corruptedFilesFound << std::setw(23 - corruptedFilesFound.length()) << BLUE << u8"║" << std::endl;
    std::cout << BLUE << u8"║" << GREEN << " Files Repaired: " << RESET << corruptedFilesFixed << std::setw(30 - corruptedFilesFixed.length()) << BLUE << u8"║" << std::endl;
    std::cout << BLUE << u8"║" << GREEN << " Files Failed Repair: " << RESET << corruptedFilesfailed << std::setw(25 - corruptedFilesfailed.length()) << BLUE << u8"║" << std::endl;
    std::cout << BLUE << u8"╟──────────────────────────────────────────╢" << std::endl;
    std::cout << BLUE << u8"║" << RED << "                 Other                    " << BLUE << u8"║" << RESET << std::endl;
    std::cout << BLUE << u8"╟──────────────────────────────────────────╢" << std::endl;
    std::cout << BLUE << u8"║" << GREEN << " Miscellaneous Repairs: " << RESET << "Completed" << std::setw(14) << BLUE << u8"║" << std::endl;
    std::cout << BLUE << u8"║" << GREEN << " Drive Defragment: " << RESET << driveDefragment << std::setw(28 - driveDefragment.length()) << BLUE << u8"║" << std::endl;
    std::cout << BLUE << u8"╚══════════════════════════════════════════╝" << std::endl;

    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);

    std::tm local_time;
#if defined(_WIN32)
    localtime_s(&local_time, &t);
#else
    localtime_r(&t, &local_time);
#endif

    char* userprofile;
    size_t len;
    errno_t err = _dupenv_s(&userprofile, &len, "USERPROFILE");
    std::filesystem::path userPath(userprofile);

    std::filesystem::create_directories(userPath / "AppData" / "Local" / "System Savior" / "Logs");
    std::stringstream filename_ss;
    filename_ss << userPath / "AppData" / "Local" / "System Savior" / "Logs" / "Log_" << std::put_time(&local_time, "%Y-%m-%d_%H-%M-%S") << ".txt";
    std::string filename = filename_ss.str();

    filename.erase(std::remove(filename.begin(), filename.end(), '\"'), filename.end());

    std::ofstream outfile;
    outfile.open(filename);
    outfile << u8"╔══════════════════════════════════════════╗";
    outfile << u8"\n║            Clutter Removal               ║";
    outfile << u8"\n╟──────────────────────────────────────────╢";
    outfile << u8"\n║" << " Files Removed: " << filesRemoved << std::setw(filesRemovedLocation-2) << u8"║";
    outfile << u8"\n║" << " Storage Saved: " << FormatStorage(storageCleared) << std::setw(storageLocation-2) << u8"║";
    outfile << u8"\n╟──────────────────────────────────────────╢";
    outfile << u8"\n║      System Files & Registry Repair      " << u8"║";
    outfile << u8"\n╟──────────────────────────────────────────╢";
    outfile << u8"\n║ Files Scanned: " << corruptedFilesScanned << std::setw(29 - corruptedFilesScanned.length()) << u8"║";
    outfile << u8"\n║ Corrupted Files Found: " << corruptedFilesFound << std::setw(21 - corruptedFilesFound.length()) << u8"║";
    outfile << u8"\n║ Files Repaired: " << corruptedFilesFixed << std::setw(28 - corruptedFilesFixed.length()) << u8"║";
    outfile << u8"\n║ Files Failed Repair: " << corruptedFilesfailed << std::setw(23 - corruptedFilesfailed.length()) << u8"║";
    outfile << u8"\n╟──────────────────────────────────────────╢";
    outfile << u8"\n║                 Other                    ║";
    outfile << u8"\n╟──────────────────────────────────────────╢";
    outfile << u8"\n║ Miscellaneous Repairs: Completed" << std::setw(12) << u8"║";
    outfile << u8"\n║ Drive Defragment: " << driveDefragment << std::setw(26 - driveDefragment.length()) << u8"║";
    outfile << u8"\n╚══════════════════════════════════════════╝";
    outfile.close();

    free(userprofile);

    for (size_t pos = filename.find("\\\\"); pos != std::string::npos; pos = filename.find("\\\\", pos)) {
        filename.replace(pos, 2, "\\");
    }

    std::cout << "\nResults have been logged to: \"" << filename << "\"" << std::endl;
    std::cout << "You may now safely close the program." << std::endl;

    while (true) {}
}

void Setup()
{
    char* userprofile;
    size_t len;
    errno_t err = _dupenv_s(&userprofile, &len, "USERPROFILE");
    std::filesystem::path userPath(userprofile);

    std::filesystem::create_directories(userPath /"AppData" / "Local" / "System Savior" / "Logs");

    free(userprofile);
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleTitle(L"System Savior");
    HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(consoleHandle, &cursorInfo);
    cursorInfo.bVisible = false;
    SetConsoleCursorInfo(consoleHandle, &cursorInfo);

    DWORD consoleMode;
    GetConsoleMode(consoleHandle, &consoleMode);
    SetConsoleMode(consoleHandle, consoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);


    std::cout << YELLOW << "System Savior has started, please do not close the program.\n" << RESET << std::endl;

    Setup();

    if (!IsAdmin())
    {
        std::cout << RED << "WARNING: You are not running the program in administrator and due to this, many important steps will be skipped. It is recommended to restart the program in administrator mode to remove limitations!\n" << RESET << std::endl;
    }
    MiscCommands();
    DriveDefragment();
    SfcScan();
    ClearUnnecessaryFiles();

    cursorInfo.bVisible = true;
    SetConsoleCursorInfo(consoleHandle, &cursorInfo);

    FinalReport();

    // DISM /Online /Cleanup-Image /CheckHealth
    // DISM /Online /Cleanup-Image /ScanHealth
    // dism /online /cleanup-image /restorehealth
    // DISM /online /cleanup-image /startcomponentcleanup
    // chkdsk /scan
    // defrag
}