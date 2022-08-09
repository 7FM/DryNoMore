#pragma once

#include <vector>
#include <string>

class Settings;
class Status;

std::string generateTable(const std::vector<std::vector<std::string>> &table);

std::string generateWaterSettingsTable(const Settings &settings);
std::string generateMoistSettingsTable(const Settings &settings);
std::string generateSettingsTable(const Settings &settings);

std::string generateStatusTable(const Status &status);
