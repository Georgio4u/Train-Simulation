#ifndef TRAIN_H 
#define TRAIN_H

#include <iostream>
#include <random>
#include <cmath>
#include <queue>
#include <iomanip>
#include <string>
#include <fstream>
#include <vector>
using namespace std;

// Range for train unload time
#define T_UNLOAD_A 3.5 
#define T_UNLOAD_B 4.5
// Range for crew arrival time
#define C_ARRIVE_A 2.5
#define C_ARRIVE_B 3.5
// Range for crews remain shift time
#define C_REMAIN_A 6.0
#define C_REMAIN_B 11.0
// Max crew shift time
#define SHIFT_TIME 12.0

/* Data Structures */

// Train status can either be arrived, in the train queue, 
// in service (at dock), or departed respectively   
enum tStat { ARRIVED, INQ, SERVICE, DEPARTED };

// Dock status can either be idle with no train, idle with a hogged 
// out train, or busy unloading a train respectively
enum dStat { IDLE, IDLE_HOGGED, BUSY };

// Crew status can either be on the clock, or hogged out respectively
enum cStat { ON_CLOCK, HOGGED_OUT };

// The dock class
class Dock
{
    public:
    dStat dockStatus = IDLE;// The status of the dock, beings empty
    double prevTime = 0.0;  // The time to be compared with globalTime
    double idleTime = 0.0;  // Keeps track of total idle time
    double busyTime = 0.0;  // Keeps track of total busy time
    double idleHogTime = 0.0;// Keeps track of total hogged out idle time 
} dock;

// Train class 
class Train
{
    public:
    int tid;                // The trains ID
    int cid;                // The ID of the crew operating the train
    int hogouts;            // The number of times a crew has hogged out on this train
    double arrivalTime;     // The start time of the trains time in service
    double endQTime;        // The time the train leaves the train queue
    double endDockTime;     // The time the train leaves the dock
    double crewArrivalTime; // The time the initial or new crew arrived at
    double timeTillHogout;  // The time left before the crew has to hog out 
    double unloadTime;      // Time it will take the train to unload at the dock
    tStat trainStatus;      // The status the train is in
    cStat crewStatus;       // The status the crew is in 

    // Used to construct the train class
    Train(int trainID, int crewID, double aTime, double hTime, double uTime )
    {
        tid = trainID;
        cid = crewID;
        hogouts = 0;
        arrivalTime = endQTime = endDockTime = crewArrivalTime = aTime;
        timeTillHogout = hTime;
        unloadTime = uTime;
        trainStatus = ARRIVED;
        crewStatus = ON_CLOCK;
    }
};

// The event class
class Event
{
    public:
    void (*function)(Train*);   // Pointer to an event function
    Train *train;               // Pointer to the train tied to the event
    double eventTime;           // The time the event pops

    // Construct the event object
    Event( void (*func)(Train*), Train *t, double time )
    {
        function = func;
        train = t;
        eventTime = time;
    }
};

// Used to define comparator for priority queue to prioritize by event time
struct comp
{
    bool operator()(const Event *a, const Event *b)
    {
        return a->eventTime > b->eventTime;
    }
};

// Statistics structure to hold data
struct Stat
{
    int numOfTrainsServed = 0;      // Train departure counter
    double sumOfTrainTimes = 0.0;   // Sum of all trains in system time
    double maxOfTrainTimes = 0.0;   // Max of all trains in system time
    double sumOfInQTimes = 0.0;     // Sum of all trains in queue time
    unsigned int maxTrainsInQ = 0;  // Max of all trains in queue time
    vector<int> histogram = vector<int>( 100 ); // Hogout histogram
} statistics;

// Structure used for organizing pre-made train schedules
struct TrainSchedule
{
    double arrivalTime;     // First, arrival time of the train
    double unloadTime;      // Second, unload time of the train
    double remainingCrewHours;// Third, remaining crew hours for that train
};

/* Utility functions */

// Event handler: pops the next event in order to perform its action
void checkEvent();

// Generate a value given a rate using a Poisson Process (optimized version from lecture) 
double genPoissonProc( int rate );

// Generate a uniformly random variable between a and b 
double genUniform( double a, double b );

// Generates or fetches pre-made crew arrival time.
// Checks global pre-made flag to decide.
double getCrewArrivalTime();

// Generates or fetches pre-made train values (times).
// Checks global pre-made flag to decide.
// Returns a reference to train value structure
TrainSchedule getTrainValues();

// Print function of the statistics of the simulation
void printStatistics();

// Will schedule the next event by its event time by using a priority queue
void schedule( void (*eventFunc)(Train*), Train *train, double time );

// Opens pre-made train schedules file and crew arrival time file 
// and stores contents into a queue. (2 queues)
void storePremades( string trainFile, string crewFile );

// Will accumulate time for different dock statuses
void updateDockTimes();

// Updates the statistics structure 
void updateStat( Train *train );

/* Action Functions */ 

// The event where a train arrives to the dock
// (Arrival time within train class is assumed to be absolute!)
void arrival( Train *currentTrain );

// The event where the train has finished unloading, and is leaving the dock. 
// Can only truly depart if train is not currently hogged out.
void departure( Train *train );

// Event for when the trains crew has to hogout.
// Can either be done in the queue or while train is in dock.
void hogout( Train *train );

// The event where the new crew has arrived to take over a train that has been hogged out
// It will schedule the next hogout for new crew only if train is in queue (when in service
// train will depart before hogging out)
void newCrew( Train *train );

// This implies the train in front of a nonempty queue was was also not hogged
// so it can be moved into dock to be unloaded and depart
void startService( Train *train );

#endif
