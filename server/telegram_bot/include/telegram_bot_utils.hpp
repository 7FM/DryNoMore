#pragma once

#include <string>
#include <vector>

class Settings;
class Status;

std::string generateTable(
    const std::vector<std::vector<std::string>> &table,
    const std::vector<bool> &printLeftWhitespace = std::vector<bool>(),
    const std::vector<bool> &printRightWhitespace = std::vector<bool>());

std::string generateWaterSettingsTable(const Settings &settings);
std::string generateMoistSettingsTable(const Settings &settings);
std::string generateSettingsTable(const Settings &settings);

std::string generateStatusTable(const Status &status);
