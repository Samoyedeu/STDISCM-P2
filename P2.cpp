#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <chrono>
#include <random>
#include <mutex>

using namespace std;

// Global vector to track each dungeon instance's status.
vector<string> dungeonStatus;
mutex statusMutex; // Protects dungeonStatus
mutex coutMutex;   // Protects console output

// Helper function to print the overall status of all dungeons.
void printDungeonStatuses() {
    lock_guard<mutex> lock(statusMutex);
    cout << "\nCurrent Dungeons Status:" << endl;
    for (size_t i = 0; i < dungeonStatus.size(); i++) {
        cout << "Dungeon " << i + 1 << ": " << dungeonStatus[i] << endl;
    }
}

// Helper function to trim leading and trailing whitespace.
void trim(string &s) {
    size_t start = s.find_first_not_of(" \t");
    size_t end = s.find_last_not_of(" \t");
    if (start == string::npos) {
        s = "";
    } else {
        s = s.substr(start, end - start + 1);
    }
}

// Read configuration values from file.
bool readConfigFile(int &numDungeons, int &numTanks, int &numHealers, int &numDPS, int &minDungeonTime, int &maxDungeonTime) {
    const string fileName = "config.txt";
    ifstream file(fileName);
    if (!file.is_open()) {
        cerr << "Error: Unable to open configuration file: " << fileName << endl;
        return false;
    }
    
    // Check if the config file is empty.
    if (file.peek() == EOF) {
        cerr << "Error: Configuration file is empty." << endl;
        return false;
    }
    
    string line;
    while (getline(file, line)) {
        size_t pos = line.find('=');
        if (pos == string::npos)
            continue; // skip lines without an '=' sign
        
        string key = line.substr(0, pos);
        string value = line.substr(pos + 1);
        trim(key);
        trim(value);
        
        try {
            if (key == "n") {
                numDungeons = stoi(value);
                if (numDungeons < 1) {
                    cerr << "Error: n (max concurrent instances) must be at least 1." << endl;
                    return false;
                }
            } else if (key == "t") {
                numTanks = stoi(value);
                if (numTanks < 1) {
                    cerr << "Error: t (number of tank players) must be at least 1." << endl;
                    return false;
                }
            } else if (key == "h") {
                numHealers = stoi(value);
                if (numHealers < 1) {
                    cerr << "Error: h (number of healer players) must be at least 1." << endl;
                    return false;
                }
            } else if (key == "d") {
                numDPS = stoi(value);
                if (numDPS < 1) {
                    cerr << "Error: d (number of DPS players) must be at least 1." << endl;
                    return false;
                }
            } else if (key == "t1") {
                minDungeonTime = stoi(value);
                if (minDungeonTime <= 0) { // t1 must be greater than 0
                    cerr << "Error: t1 (min time) must be greater than 0." << endl;
                    return false;
                }
            } else if (key == "t2") {
                maxDungeonTime = stoi(value);
                if (maxDungeonTime <= 0) { // t2 must be greater than 0
                    cerr << "Error: t2 (max time) must be greater than 0." << endl;
                    return false;
                }
                if (maxDungeonTime < minDungeonTime) {
                    cerr << "Error: t2 (max time) must be greater than or equal to t1." << endl;
                    return false;
                }
                if (maxDungeonTime > 15) {
                    cerr << "Error: t2 (max time) must be less than or equal to 15." << endl;
                    return false;
                }
            } else {
                cout << "Warning: Unknown configuration key \"" << key << "\". Skipping." << endl;
            }
        } catch (...) {
            cerr << "Error: Invalid value for key \"" << key << "\"." << endl;
            return false;
        }
    }
    return true;
}

// Attempt to form a party: 1 tank, 1 healer, 3 DPS.
// Returns true if a party is formed (and decrements counts), false otherwise.
bool formParty(int &numTanks, int &numHealers, int &numDPS) {
    if (numTanks < 1 || numHealers < 1 || numDPS < 3)
        return false;
    numTanks--;
    numHealers--;
    numDPS -= 3;
    return true;
}

// Structure to hold statistics for each dungeon instance.
struct DungeonStats {
    int partiesServed;
    int totalTime;
    DungeonStats() : partiesServed(0), totalTime(0) {}
};

// Global vector to store statistics for each dungeon instance.
vector<DungeonStats> instanceStats;
mutex statsMutex;  // Protects instanceStats

// Function that simulates a dungeon instance processing one party.
void dungeonInstance(int instanceId, int minTime, int maxTime) {
    // Create a random generator for the dungeon run time.
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(minTime, maxTime);
    int sleepTime = dis(gen);
    
    // Simulate the dungeon run.
    this_thread::sleep_for(chrono::seconds(sleepTime));
    
    {
        // Update statistics in a thread-safe manner.
        lock_guard<mutex> lock(statsMutex);
        instanceStats[instanceId].partiesServed++;
        instanceStats[instanceId].totalTime += sleepTime;
    }
    
    {
        // Mark this dungeon instance as empty.
        lock_guard<mutex> lock(statusMutex);
        dungeonStatus[instanceId] = "empty";
    }
    
    {
        // Print the updated overall dungeon status.
        lock_guard<mutex> lock(coutMutex);
        printDungeonStatuses();
    }
}

int main() {
    int numDungeons = 0, numTanks = 0, numHealers = 0, numDPS = 0;
    int minDungeonTime = 0, maxDungeonTime = 0;
    
    cout << "Reading config from config.txt" << endl;
    if (!readConfigFile(numDungeons, numTanks, numHealers, numDPS, minDungeonTime, maxDungeonTime)) {
        return 1;
    }
    
    // Initialize dungeon statistics and statuses for each instance.
    instanceStats.resize(numDungeons);
    dungeonStatus.resize(numDungeons, "empty");
    
    vector<thread> dungeonThreads;
    int instanceId = 0; // Round-robin assignment of dungeon instances.
    
    // Process parties until we run out of enough players.
    while (formParty(numTanks, numHealers, numDPS)) {
        // If we have reached the max concurrent dungeon instances, wait for them to finish.
        if (dungeonThreads.size() >= static_cast<size_t>(numDungeons)) {
            for (auto &th : dungeonThreads) {
                if (th.joinable())
                    th.join();
            }
            dungeonThreads.clear();
        }
        
        // Mark the current dungeon instance as active.
        {
            lock_guard<mutex> lock(statusMutex);
            dungeonStatus[instanceId] = "active";
        }
        
        {
            // Print the overall status block immediately after queueing players.
            lock_guard<mutex> lock(coutMutex);
            cout << "\nQueueing up players" << endl;
            printDungeonStatuses();
        }
        
        // Launch a dungeon instance thread for the formed party.
        dungeonThreads.emplace_back(dungeonInstance, instanceId, minDungeonTime, maxDungeonTime);
        
        // Cycle through dungeon instance IDs.
        instanceId = (instanceId + 1) % numDungeons;
    }
    
    // Wait for any remaining dungeon threads to finish.
    for (auto &th : dungeonThreads) {
        if (th.joinable())
            th.join();
    }
    
    // Output summary statistics for each dungeon instance.
    cout << "\nDungeon Instance Summary:" << endl;
    for (int i = 0; i < numDungeons; i++) {
        cout << "Dungeon " << i + 1 
             << " served " << instanceStats[i].partiesServed 
             << " parties, total time = " << instanceStats[i].totalTime 
             << " seconds." << endl;
    }
    
    // Print out any leftover players.
    cout << "\nLeftover players:" << endl;
    cout << "Tanks: " << numTanks << endl;
    cout << "Healers: " << numHealers << endl;
    cout << "DPS: " << numDPS << endl;
    
    return 0;
}
