#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <chrono>
#include <random>
#include <mutex>

using namespace std;

mutex coutMutex;      // Protects console output
mutex playerMutex;    // Protects global player counts
mutex statsMutex;     // Protects instanceStats

// Global configuration values.
int numDungeons = 0, numTanks = 0, numHealers = 0, numDPS = 0;
int minDungeonTime = 0, maxDungeonTime = 0;

// Structure to hold statistics and status for each dungeon instance.
struct DungeonStats {
    bool active;
    int partiesServed;
    int totalTime;
    DungeonStats() : active(false), partiesServed(0), totalTime(0) {}
};

// Global vector to store statistics and status for each dungeon instance.
vector<DungeonStats> instanceStats;

// Helper function to print the overall status of all dungeon instances.
void printDungeonStatuses() {
    lock_guard<mutex> lock(statsMutex);
    cout << "\nCurrent Dungeons Status:" << endl;
    for (size_t i = 0; i < instanceStats.size(); i++) {
        cout << "Dungeon " << i + 1 << ": " 
             << (instanceStats[i].active ? "active" : "empty") << endl;
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
bool readConfigFile() {
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
            continue; // Skip lines without an '=' sign.
        
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
                if (minDungeonTime <= 0) {
                    cerr << "Error: t1 (min time) must be greater than 0." << endl;
                    return false;
                }
            } else if (key == "t2") {
                maxDungeonTime = stoi(value);
                if (maxDungeonTime <= 0) {
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

// Persistent dungeon instance thread function that continuously processes parties.
void queueParty(int instanceId) {
    // Create a random generator for the dungeon run time.
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(minDungeonTime, maxDungeonTime);
    
    while (true) {
        // Attempt to form a party by deducting players.
        {
            lock_guard<mutex> lock(playerMutex);
            if (numTanks < 1 || numHealers < 1 || numDPS < 3) {
                break; // No more parties can be formed.
            }
            numTanks--;
            numHealers--;
            numDPS -= 3;
        }
        
        // Mark this dungeon instance as active.
        {
            lock_guard<mutex> lock(statsMutex);
            instanceStats[instanceId].active = true;
        }
        
        {
            lock_guard<mutex> lock(coutMutex);
            cout << "\nQueueing up players for Dungeon Instance " << instanceId + 1 << endl;
            printDungeonStatuses();
        }
        
        // Simulate the dungeon run.
        int dungeonTime = dis(gen);
        this_thread::sleep_for(chrono::seconds(dungeonTime));
        
        // Update statistics and mark instance as empty.
        {
            lock_guard<mutex> lock(statsMutex);
            instanceStats[instanceId].partiesServed++;
            instanceStats[instanceId].totalTime += dungeonTime;
            instanceStats[instanceId].active = false;
        }
        
        {
            lock_guard<mutex> lock(coutMutex);
            cout << "\nDungeon Instance " << instanceId + 1 << " finished processing a party." << endl;
            printDungeonStatuses();
        }
    }
    
    {
        lock_guard<mutex> lock(coutMutex);
        cout << "\nDungeon Instance " << instanceId + 1 << " is closing as no more parties can be formed." << endl;
    }
}

int main() {
    cout << "Reading config from config.txt" << endl;
    if (!readConfigFile()) {
        return 1;
    }
    
    // Resize the instanceStats vector based on the number of dungeon instances.
    instanceStats.resize(numDungeons);
    
    vector<thread> dungeonThreads;
    // Create persistent dungeon instance threads.
    for (int i = 0; i < numDungeons; i++) {
        dungeonThreads.emplace_back(queueParty, i);
    }
    
    // Wait for all dungeon threads to finish.
    for (auto &th : dungeonThreads) {
        if (th.joinable())
            th.join();
    }
    
    // Output summary statistics for each dungeon instance.
    cout << "\nDungeon Instance Summary:" << endl;
    int totalCountPartiesServed = 0;
    for (int i = 0; i < numDungeons; i++) {
        totalCountPartiesServed += instanceStats[i].partiesServed;
        cout << "Dungeon " << i + 1 
             << " served " << instanceStats[i].partiesServed 
             << " parties, total time = " << instanceStats[i].totalTime 
             << " seconds." << endl;
    }
    cout << "Total count of parties served: " << totalCountPartiesServed << endl;
    
    // Print out any leftover players. 
    cout << "\nLeftover players:" << endl;
    cout << "Tanks: " << numTanks << endl;
    cout << "Healers: " << numHealers << endl;
    cout << "DPS: " << numDPS << endl;
    
    return 0;
}
