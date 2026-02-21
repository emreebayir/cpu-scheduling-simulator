#include <iostream>
#include <vector>
#include <string>
#include <deque>
#include <map>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <climits>

using namespace std;


enum ProcessState { NEW, READY, RUNNING, BLOCKED, TERMINATED };
enum InstructionType { CPU, IO, REQ, REL };
enum BlockReason { NONE, WAITING_IO, WAITING_RESOURCE };

struct Instruction {
    InstructionType type;
    int val;   
    int count; 
};

struct Process {
    string pid;
    int arrivalTime;
    int priority; 
    vector<Instruction> instructions;
    size_t pc = 0; 

    int remainingTimeCurrentOp = 0; 
    ProcessState state = NEW;
    BlockReason blockReason = NONE;
    int blockedForResourceId = -1; 

    
    int startTime = -1; 
    int finishTime = 0;
    int totalCpuTime = 0;
    int totalIoTime = 0;
    int lastReadyTime = 0; 

    int queueLevel = 0; 
};

struct Resource {
    int id;
    int capacity;
    int available;
    map<string, int> allocation;
};


class ResourceManager {
public:
    map<int, Resource> resources;
    map<int, deque<Process*>> waitingQueues;

    void init(int m, const vector<int>& caps) {
        for (int i = 0; i < m; ++i) {
            resources[i + 1] = { i + 1, caps[i], caps[i], {} };
        }
    }

    bool request(Process* p, int rId, int count) {
        if (resources.find(rId) == resources.end()) return false;

        if (resources[rId].available >= count) {
            resources[rId].available -= count;
            resources[rId].allocation[p->pid] += count;
            return true;
        }
        else {
            waitingQueues[rId].push_back(p);
            return false;
        }
    }

    void release(Process* p, int rId, int count, deque<Process*>& readyQueue) {
        if (resources.find(rId) == resources.end()) return;

        resources[rId].available += count;
        if (p) {
            resources[rId].allocation[p->pid] -= count;
            if (resources[rId].allocation[p->pid] <= 0) {
                resources[rId].allocation.erase(p->pid);
            }
        }

        auto& q = waitingQueues[rId];
        while (!q.empty()) {
            Process* waiter = q.front();
            int needed = waiter->instructions[waiter->pc].count;

            if (resources[rId].available >= needed) {
                resources[rId].available -= needed;
                resources[rId].allocation[waiter->pid] += needed;

                q.pop_front();
                waiter->state = READY;
                waiter->blockReason = NONE;
                waiter->blockedForResourceId = -1;
                waiter->pc++; 
                readyQueue.push_back(waiter);

                cout << "[UNBLOCK] Process " << waiter->pid << " got Resource " << rId << endl;
            }
            else {
                break;
            }
        }
    }

    void releaseAllResourcesOfProcess(Process* p, deque<Process*>& readyQueue) {
        for (auto& pair : resources) {
            int rId = pair.first;
            if (pair.second.allocation.count(p->pid)) {
                int count = pair.second.allocation[p->pid];
                if (count > 0) {
                    cout << "[RECOVERY] Releasing " << count << " of R" << rId << " from aborted " << p->pid << endl;
                    release(p, rId, count, readyQueue);
                }
            }
        }
    }
};

class Scheduler {
public:
    string algorithm;
    int quantum;
    int currentTime = 0;

    deque<Process*> readyQueue;
    deque<Process*> mlfq[3];

    Scheduler(string alg, int q) : algorithm(alg), quantum(q) {}
   
    void setTime(int t) { currentTime = t; }

    void addProcess(Process* p) {
        p->state = READY;
        p->lastReadyTime = currentTime;

        if (algorithm == "mlfq") {
            mlfq[p->queueLevel].push_back(p);
        }
        else if (algorithm == "prio") {
            readyQueue.push_back(p);
            sort(readyQueue.begin(), readyQueue.end(), [](Process* a, Process* b) {
                if (a->priority == b->priority) return a->arrivalTime < b->arrivalTime;
                return a->priority < b->priority;
                });
        }
        else {
            readyQueue.push_back(p);
        }
    }

    Process* getNextProcess() {
        if (algorithm == "rr" || algorithm == "prio") {
            if (readyQueue.empty()) return nullptr;
            Process* p = readyQueue.front();
            readyQueue.pop_front();
            return p;
        }
        else if (algorithm == "mlfq") {
            for (int i = 0; i < 3; ++i) {
                if (!mlfq[i].empty()) {
                    Process* p = mlfq[i].front();
                    mlfq[i].pop_front();
                    return p;
                }
            }
        }
        return nullptr;
    }

    bool checkQuantum(Process* p, int executed) {
        int limit = quantum;
        if (algorithm == "mlfq") {
            limit = quantum * (1 << p->queueLevel);  
        }
        else if (algorithm == "prio") {
            return false;
        }

        if (executed >= limit) {
            if (algorithm == "mlfq") {
                if (p->queueLevel < 2) p->queueLevel++;
            }
            return true;
        }
        return false;
    }

    void applyAging() {
        if (algorithm == "prio") {
            bool changed = false;
            for (auto p : readyQueue) {
                if (currentTime - p->lastReadyTime > 50) {
                    if (p->priority > 0) {
                        p->priority--;
                        p->lastReadyTime = currentTime;
                        changed = true;
                    }
                }
            }
            if (changed) {
                sort(readyQueue.begin(), readyQueue.end(), [](Process* a, Process* b) {
                    return a->priority < b->priority;
                    });
            }
        }

        if (algorithm == "mlfq" && currentTime > 0 && currentTime % 200 == 0) {
            bool boosted = false;
            for (int i = 1; i < 3; ++i) {
                while (!mlfq[i].empty()) {
                    Process* p = mlfq[i].front();
                    mlfq[i].pop_front();
                    p->queueLevel = 0;
                    mlfq[0].push_back(p);
                    boosted = true;
                }
            }
            if (boosted) cout << "Time " << currentTime << " [BOOST] All MLFQ processes moved to Level 0" << endl;
        }
    }
};

class Simulator {
    vector<Process> processes;
    vector<Process*> allProcPtrs;
    ResourceManager rm;
    Scheduler* sched;
    int time = 0;

public:
    Simulator(string alg, int q, const string& inputFile) {
        sched = new Scheduler(alg, q);
        loadInput(inputFile);
    }

    void loadInput(const string& filename) {
        istream* input = &cin;
        ifstream file;
        if (!filename.empty()) {
            file.open(filename);
            if (!file.is_open()) {
                cerr << "Error: Cannot open file " << filename << endl;
                exit(1);
            }
            input = &file;
        }

        int M;
        if (!(*input >> M)) return;
        vector<int> caps(M);
        for (int i = 0; i < M; ++i) *input >> caps[i];
        rm.init(M, caps);

        string token;
        while (*input >> token) {
            if (token == "END") break;
            Process p;
            p.pid = token;
            *input >> p.arrivalTime >> p.priority;

            string op;
            while (*input >> op && op != "END") {
                Instruction instr;
                if (op == "CPU") {
                    instr.type = CPU;
                    *input >> instr.val;
                }
                else if (op == "IO") {
                    instr.type = IO;
                    *input >> instr.val;
                }
                else if (op.rfind("REQ", 0) == 0) {
                    instr.type = REQ;
                    instr.val = stoi(op.substr(3));
                    *input >> instr.count;
                }
                else if (op.rfind("REL", 0) == 0) {
                    instr.type = REL;
                    instr.val = stoi(op.substr(3));
                    *input >> instr.count;
                }
                p.instructions.push_back(instr);
            }
            processes.push_back(p);
        }

        for (size_t i = 0; i < processes.size(); ++i) {
            allProcPtrs.push_back(&processes[i]);
        }
    }

    void checkAndResolveDeadlock(Process*& runningProcess) {
        bool isAnyRunning = (runningProcess != nullptr);
        bool isAnyReady = !sched->readyQueue.empty();
        if (!isAnyReady) {
            for (int i = 0; i < 3; ++i) if (!sched->mlfq[i].empty()) isAnyReady = true;
        }

        if (!isAnyRunning && !isAnyReady) {
            vector<Process*> blockedOnResource;
            for (auto p : allProcPtrs) {
                if (p->state == BLOCKED && p->blockReason == WAITING_RESOURCE) {
                    blockedOnResource.push_back(p);
                }
            }

            bool anyIO = false;
            for (auto p : allProcPtrs) if (p->state == BLOCKED && p->blockReason == WAITING_IO) anyIO = true;

            if (!blockedOnResource.empty() && !anyIO) {
                cout << "\n*** DEADLOCK DETECTED at time " << time << " ***" << endl;
                Process* victim = blockedOnResource[0];
                cout << "[DEADLOCK RECOVERY] Aborting process " << victim->pid << endl;

                rm.releaseAllResourcesOfProcess(victim, sched->readyQueue);

                victim->state = TERMINATED;
                victim->finishTime = time;
            }
        }
    }

    void run() {
        int completedCount = 0;
        Process* runningProcess = nullptr;
        int currentBurstExecuted = 0;

        cout << "--- Timeline Log ---" << endl;

        while (completedCount < processes.size()) {

            for (auto p : allProcPtrs) {
                if (p->state == NEW && p->arrivalTime == time) {
                    sched->addProcess(p);
                }
            }

            for (auto p : allProcPtrs) {
                if (p->state == BLOCKED && p->blockReason == WAITING_IO) {
                    p->remainingTimeCurrentOp--;
                    p->totalIoTime++;
                    if (p->remainingTimeCurrentOp <= 0) {
                        p->state = READY;
                        p->blockReason = NONE;
                        p->pc++;
                        sched->addProcess(p);
                    }
                }
            }

            sched->setTime(time);
            sched->applyAging();

            if (runningProcess == nullptr) {
                runningProcess = sched->getNextProcess();
                currentBurstExecuted = 0;

                if (runningProcess) {
                    runningProcess->state = RUNNING;
                    if (runningProcess->startTime == -1) runningProcess->startTime = time;

                    if (runningProcess->pc < runningProcess->instructions.size()) {
                        Instruction& inst = runningProcess->instructions[runningProcess->pc];
                        if (inst.type == CPU && runningProcess->remainingTimeCurrentOp <= 0) {
                            runningProcess->remainingTimeCurrentOp = inst.val;
                        }
                    }
                }
            }

            checkAndResolveDeadlock(runningProcess);

            if (runningProcess && runningProcess->state == TERMINATED) {
                runningProcess = nullptr;
                completedCount++;
            }

            if (runningProcess) {
                if (runningProcess->pc >= runningProcess->instructions.size()) {
                    runningProcess->state = TERMINATED;
                    runningProcess->finishTime = time;
                    runningProcess = nullptr;
                    completedCount++;
                    continue;
                }

                Instruction& inst = runningProcess->instructions[runningProcess->pc];

                if (inst.type == CPU) {
                    cout << "Time " << time << ": " << runningProcess->pid << " RUNNING" << endl;
                    runningProcess->remainingTimeCurrentOp--;
                    runningProcess->totalCpuTime++;
                    currentBurstExecuted++;

                    if (runningProcess->remainingTimeCurrentOp <= 0) {
                        runningProcess->pc++;
                        runningProcess->state = READY;
                        if (runningProcess->pc >= runningProcess->instructions.size()) {
                            runningProcess->state = TERMINATED;
                            runningProcess->finishTime = time + 1;
                            completedCount++;
                            runningProcess = nullptr;
                        }
                        else {
                            sched->addProcess(runningProcess);
                            runningProcess = nullptr;
                        }
                    }
                    else {
                        if (sched->checkQuantum(runningProcess, currentBurstExecuted)) {
                            sched->addProcess(runningProcess);
                            runningProcess = nullptr;
                        }
                    }
                }
                else if (inst.type == IO) {
                    runningProcess->state = BLOCKED;
                    runningProcess->blockReason = WAITING_IO;
                    runningProcess->remainingTimeCurrentOp = inst.val;
                    cout << "Time " << time << ": " << runningProcess->pid << " BLOCK (IO)" << endl;
                    runningProcess = nullptr;
                }
                else if (inst.type == REQ) {
                    cout << "Time " << time << ": " << runningProcess->pid << " REQUEST R" << inst.val << " (" << inst.count << ")" << endl;
                    if (rm.request(runningProcess, inst.val, inst.count)) {
                        runningProcess->pc++;
                        sched->addProcess(runningProcess);
                    }
                    else {
                        runningProcess->state = BLOCKED;
                        runningProcess->blockReason = WAITING_RESOURCE;
                        runningProcess->blockedForResourceId = inst.val;
                        cout << "Time " << time << ": " << runningProcess->pid << " BLOCKED (Resource R" << inst.val << ")" << endl;
                    }
                    runningProcess = nullptr;
                }
                else if (inst.type == REL) {
                    cout << "Time " << time << ": " << runningProcess->pid << " RELEASE R" << inst.val << " (" << inst.count << ")" << endl;
                    rm.release(runningProcess, inst.val, inst.count, sched->readyQueue);
                    runningProcess->pc++;
                    sched->addProcess(runningProcess);
                    runningProcess = nullptr;
                }
            }
            else {
                cout << "Time " << time << ": IDLE" << endl;
            }

            time++;
        }

        printMetrics();
    }

    void printMetrics() {
        cout << "\n--- Metrics ---" << endl;
        cout << left << setw(10) << "PID"
            << setw(12) << "Turnaround"
            << setw(10) << "Waiting"
            << setw(10) << "Response"
            << setw(10) << "CPU Time"
            << setw(10) << "IO Time" << endl;

        double avgTurn = 0, avgWait = 0, avgResp = 0;
        double totalCpu = 0;
        int count = 0;

        for (auto p : allProcPtrs) {
            if (p->finishTime == 0) continue;

            int turn = p->finishTime - p->arrivalTime;
            int wait = turn - p->totalCpuTime - p->totalIoTime;
            if (wait < 0) wait = 0;

            int resp = p->startTime - p->arrivalTime;

            cout << left << setw(10) << p->pid
                << setw(12) << turn
                << setw(10) << wait
                << setw(10) << resp
                << setw(10) << p->totalCpuTime
                << setw(10) << p->totalIoTime << endl;

            avgTurn += turn;
            avgWait += wait;
            avgResp += resp;
            totalCpu += p->totalCpuTime;
            count++;
        }

        if (count > 0) {
            cout << "\nAverages:" << endl;
            cout << "Turnaround: " << avgTurn / count << endl;
            cout << "Waiting:    " << avgWait / count << endl;
            cout << "Response:   " << avgResp / count << endl;
            cout << "CPU Util:   " << (totalCpu / time) * 100.0 << "%" << endl;
            cout << "Throughput: " << (double)count / time << " proc/unit time" << endl;
        }
    }
};

int main(int argc, char* argv[]) {
    string alg = "rr";
    int quantum = 10;
    string input;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--alg") {
            if (i + 1 < argc) alg = argv[++i];
        }
        else if (arg == "--q") {
            if (i + 1 < argc) quantum = stoi(argv[++i]);
        }
        else if (arg == "--input") {
            if (i + 1 < argc) input = argv[++i];
        }
    }

    if (alg != "rr" && alg != "prio" && alg != "mlfq") {
        cerr << "Unknown algorithm: " << alg << endl;
        return 1;
    }

    Simulator sim(alg, quantum, input);
    sim.run();

    cout << "\nSimulasyon tamamlandi. Cikmak icin Enter'a basin...";
    cin.get();
    return 0;
}