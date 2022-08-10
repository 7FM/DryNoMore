#include <sstream>

#include "telegram_bot_utils.hpp"
#include "types.hpp"

static size_t findMaxStringLineSize(const std::string &s) {
  size_t max = 0;
  auto it = s.find('\n');
  if (it != std::string::npos) {
    max = std::max(it, findMaxStringLineSize(s.substr(it + 1)));
  } else {
    max = s.size();
  }

  return max;
}

std::string generateTable(const std::vector<std::vector<std::string>> &table) {

  if (table.empty()) {
    return "";
  }

  std::vector<size_t> maxChars;
  maxChars.reserve(table.size());

  size_t totalWidth = 1;
  for (size_t i = 0; i < table[0].size(); ++i) {
    size_t max = 0;
    for (auto &row : table) {
      max = std::max(max, findMaxStringLineSize(row[i]));
    }
    totalWidth += max + 3;
    maxChars.push_back(max);
  }

  std::stringstream ss;
  ss << "```\n";

  auto print_multirow = [&](const std::vector<std::string> &row) {
    std::vector<std::string> newRow;
    std::vector<std::string> nextRow;
    const std::vector<std::string> *currentRow = &row;

    do {
      ss << "|";
      for (size_t i = 0; i < maxChars.size(); ++i) {
        ss << ' ';
        auto old = ss.width(maxChars[i]);
        const auto &s = (*currentRow)[i];
        auto it = s.find('\n');
        if (it != std::string::npos) {
          if (newRow.size() != i) {
            // Fill with empty strings up to i
            newRow.resize(i);
          }
          ss << s.substr(0, it);
          newRow.push_back(s.substr(it + 1));
        } else {
          ss << s;
        }
        ss.width(old);
        ss << " |";
      }
      ss << '\n';

      std::swap(nextRow, newRow);
      newRow.clear();
      currentRow = &nextRow;
    } while (!currentRow->empty());
  };

  auto print_break = [&] {
    ss.width(totalWidth);
    ss.fill('-');
    ss << '-' << '\n';
    ss.fill(' ');
  };

  ss.setf(std::ios::left, std::ios::adjustfield);
  print_break();
  print_multirow(table[0]);
  print_break();
  for (size_t i = 1; i < table.size(); ++i) {
    print_multirow(table[i]);
  }
  print_break();

  ss << "```\n";
  return ss.str();
}

std::string generateWaterSettingsTable(const Settings &settings) {
  std::vector<std::vector<std::string>> table;
  table.reserve(1 + 2);

  std::vector<std::string> row{"ID", "Min\nVal", "Max\nVal", "Warn\nThres",
                               "Empty\nThres"};
  table.push_back(std::move(row));

  for (int i = 0; i < 2; ++i) {
    row = {"W" + std::to_string(i + 1),
           std::to_string(
               settings.sensConfs[(MAX_MOISTURE_SENSOR_COUNT) + i].minValue),
           std::to_string(
               settings.sensConfs[(MAX_MOISTURE_SENSOR_COUNT) + i].maxValue),
           std::to_string(settings.waterLvlThres[i].warnThres) + " %",
           std::to_string(settings.waterLvlThres[i].emptyThres) + " %"};
    table.push_back(std::move(row));
  }

  return "Water-level Sensor Settings:\n" + generateTable(table);
}
std::string generateMoistSettingsTable(const Settings &settings) {
  std::vector<std::vector<std::string>> table;
  table.reserve(1 + settings.numPlants);

  std::vector<std::string> row{
      "ID", "Min\nVal", "Max\nVal", "Target\nMoist", "Water\nSensor", "Skip"};
  table.push_back(std::move(row));

  for (int i = 0; i < settings.numPlants; ++i) {
    uint8_t sensorIdx =
        (settings.moistSensToWaterSensBitmap[i / 8] >> (i & 7 /*aka mod 8*/)) &
        0x01;
    uint8_t skip = (settings.skipBitmap[i / 8] >> (i & 7 /*aka mod 8*/)) & 0x01;
    std::string waterSensor = sensorIdx ? "W2" : "W1";
    row = {"P" + std::to_string(i + 1),
           std::to_string(settings.sensConfs[i].minValue),
           std::to_string(settings.sensConfs[i].maxValue),
           std::to_string(settings.targetMoistures[i]) + " %",
           waterSensor,
           skip ? "true" : "false"};
    table.push_back(std::move(row));
  }

  return "Moisture Sensor Settings:\n" + generateTable(table);
}

std::string generateSettingsTable(const Settings &settings) {
  return generateMoistSettingsTable(settings) +
         generateWaterSettingsTable(settings);
}

std::string generateStatusTable(const Status &status) {
  std::vector<std::vector<std::string>> table;
  table.reserve(1 + status.numPlants);

  std::vector<std::string> row{"ID", "Moisture\nBefore", "Moisture\nAfter"};
  table.push_back(std::move(row));

  for (int i = 0; i < status.numPlants; ++i) {
    row = {"P" + std::to_string(i + 1),
           std::to_string(status.beforeMoistureLevels[i]) + " %",
           std::to_string(status.afterMoistureLevels[i]) + " %"};
    table.push_back(std::move(row));
  }

  std::string moistureSensorTable(generateTable(table));
  table.clear();

  row = {"ID", "WaterLvl\nBefore", "WaterLvl\nAfter"};
  table.push_back(std::move(row));

  for (int i = 0; i < status.numWaterSensors; ++i) {
    row = {"W" + std::to_string(i + 1),
           std::to_string(status.beforeWaterLevels[i]) + " %",
           std::to_string(status.afterWaterLevels[i]) + " %"};
    table.push_back(std::move(row));
  }

  std::string waterSensorTable(generateTable(table));

  return "Plant Status:\n" + moistureSensorTable + "Water-level Status:\n" +
         waterSensorTable;
}