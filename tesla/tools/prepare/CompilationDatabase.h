#pragma once
#include "DataStructures.h"
#include "Debug.h"
#include "Utils.h"
#include <fstream>
#include <unordered_map>

class TeslaCompilationDatabase
{
  public:
    TeslaCompilationDatabase()
    {
    }

    TeslaCompilationDatabase(const std::string& compilationDatabaseFilename)
        : databaseFilename(compilationDatabaseFilename)
    {
        ReadDatabase();
        empty = false;
    }

    bool IsEmpty()
    {
        return empty;
    }

    bool IsFileInDatabase(const std::string& filename)
    {
        return database.find(filename) != database.end();
    }

    std::vector<std::string> GetCompilationOptions(const std::string& filename)
    {
        return database[filename];
    }

  private:
    void ReadDatabase()
    {
        std::ifstream file(databaseFilename);

        if (!file)
        {
            tesla::panic("Could not open compilation database " + databaseFilename);
        }

        std::string line;

        while (std::getline(file, line))
        {
            std::stringstream iss(line);
            std::string sourceFile;
            std::string separator;
            if (!(iss >> sourceFile >> separator) || separator != "--")
            {
                tesla::panic("Compilation database malformed - line: " + line);
            }

            std::vector<std::string> opts;
            opts.push_back("--");

            std::string opt;
            while (iss >> opt)
            {
                opts.push_back(opt);
            }

            database[sourceFile] = opts;
        }

        file.close();
    }

    std::unordered_map<std::string, std::vector<std::string>> database;

    std::string databaseFilename;

    bool empty = true;
};