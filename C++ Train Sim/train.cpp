#include "train.h"

// Priority Queue of events prioritizing by event time
priority_queue<Event*, vector<Event*>, comp> eventQ;

// Train Queue, FIFO
queue<Train*> trainQ;

// Pre-made train schedules
queue<TrainSchedule> trainSchedule;

// Pre-made crew travel times
queue<double> travelTime;

// Global values used by functions
int nextTrainId;
int nextCrewId;
double globalTime;
double simTime;
int rate;
bool premade;

// Used for random number generation
random_device rd;
mt19937 gen(rd()); 

// The function that brings life to this program
int main( int argc, char* argv[] )
{
    // Check method of getting values (either pre-made or generated)
    if( argc < 3 )
    {
        simTime = 72000.0;
        rate = 10.0;
    }
    else if( string(argv[1]) == "-s" )
    {
        premade = true;
        storePremades( argv[2], argv[3] );
    }
    else if( argc == 3 )
    {
        premade = false;
        rate = atof( argv[1] );
        simTime = atof( argv[2] );
    }
    // Time zero with initial crew and train id
    globalTime = 0.0;
    nextTrainId = 0;
    nextCrewId = 0;
    // Generate INITIAL arrival, unload, hogout times
    TrainSchedule ts = getTrainValues();
    // Create a new train
    Train *trainPtr = new Train(nextTrainId++, nextCrewId++, 0, ts.remainingCrewHours, ts.unloadTime );
    // Schedule when the train will arrive and when the crew will hog out
    schedule(arrival, trainPtr, globalTime);
    /* Didn't schedule hogout because unload time is far less then crew hogout time */
    // Each arrival event should generate a new arrival, unless end conditions are met.
    // After that event queue depletion should terminate simulation or when there are no more trains to schedule
    while( (!premade && !eventQ.empty()) || (premade && !trainSchedule.empty()) )
    {
        checkEvent();
        updateDockTimes();
    }
    cout << "Time " << globalTime << ": simulation ended" << endl;
    // Simulation terminated so print statistics 
    cout << endl;
    printStatistics();

    return 0;
}

/* Utility functions */

// Event handler: pops the next event in order to perform its action
void checkEvent()
{
    // Pop the next event
    Event *e = eventQ.top();
    eventQ.pop();

    // If train has departed then skip this event and free memory
    if( e->train->trainStatus == DEPARTED )
    {
        delete e->train;
    }
    else // otherwise perform action for the event by calling the event function
    {
        globalTime = e->eventTime;
        e->function(e->train);
    }
    // Event deleted
    e->train = nullptr;
    delete e;
}

// Generate a value given a rate using a Poisson Process (optimized version from lecture) 
double genPoissonProc( int rate )
{
    //default_random_engine gen( seed ); 
    uniform_real_distribution<double> u(0, 1);
    return -log(u( gen )) * rate;
}

// Generate a uniformly random variable between a and b
double genUniform( double a, double b )
{   
   //default_random_engine gen( seed );
    uniform_real_distribution<double> u(a, b);
    return u( gen );
}

// Generates or fetches pre-made crew arrival time.
// Checks global pre-made flag to decide.
double getCrewArrivalTime()
{
    double time;
    if( premade )
    {
        // Pre-made crew travel time is stored in a queue
        time = travelTime.front();
        travelTime.pop();
    }
    else
    {
        time = genUniform( C_ARRIVE_A, C_ARRIVE_B );
    }

    return time;
}

// Generates or fetches pre-made train values (times).
// Checks global pre-made flag to decide.
// Returns a reference to train value structure
TrainSchedule getTrainValues()
{
    TrainSchedule ts;
    if( premade )
    {
        ts = trainSchedule.front();
        trainSchedule.pop();
    }
    else
    {
        ts.arrivalTime = genPoissonProc( rate );
        ts.unloadTime = genUniform( T_UNLOAD_A, T_UNLOAD_B );
        ts.remainingCrewHours = genUniform( C_REMAIN_A, C_REMAIN_B);
    }    
    return ts;
}

// Print function of the statistics of the simulation
void printStatistics()
{
    cout << "Statistics" << endl;
    cout << "----------" << endl;
    cout << "Total number of trains served: " << statistics.numOfTrainsServed << endl;
    cout << "Average time-in-system per train: " << statistics.sumOfTrainTimes / statistics.numOfTrainsServed << 'h' << endl;
    cout << "Maximum time-in-system per train: " << statistics.maxOfTrainTimes << 'h' << endl;
    cout << "Dock idle percentage: " << (dock.idleTime / globalTime)*100 << '%' << endl;
    cout << "Dock busy percentage: " << (dock.busyTime / globalTime)*100 << '%' << endl;
    cout << "Dock hogged-out percentage: " << (dock.idleHogTime / globalTime)*100 << '%' << endl;
    cout << "Time average of trains in queue: " << statistics.sumOfInQTimes / statistics.numOfTrainsServed << endl;
    cout << "Maximum number of trains in queue: " << statistics.maxTrainsInQ << endl;
    cout << "Histogram of hogout count per train:" << endl;

    // Print the histogram of hougouts
    int size = statistics.histogram.size();
    for( int i = 0; i < size; ++i )
        if( statistics.histogram.at(i) != 0 )
            cout << '[' << i << ']' << ": " << statistics.histogram.at(i) << endl;
}

// Will schedule the next event by its event time by using a priority queue
void schedule( void (*eventFunc)(Train*), Train *train, double time )
{
    eventQ.push( new Event(eventFunc, train, time) );
}

// Opens pre-made train schedules file and crew arrival time file 
// and stores contents into a queue. (2 queues)
void storePremades( string trainFile, string crewFile )
{
    ifstream in;
    in = ifstream( trainFile );
    TrainSchedule ts;
    for( int i = 0; in >> ts.arrivalTime >> ts.unloadTime >> ts.remainingCrewHours; ++i )
        trainSchedule.push( ts );
    in.close();
    in = ifstream( crewFile );
    double t;
    for( int i = 0; in >> t; ++i )
        travelTime.push( t );
    in.close();
}

// Will accumulate time for different dock statuses
void updateDockTimes()
{
    switch( dock.dockStatus )
    {
        // Accumulate dock idle time
        case IDLE:  dock.idleTime += (globalTime - dock.prevTime);
                    break;
        // Accumulate dock busy time
        case BUSY:  dock.busyTime += (globalTime - dock.prevTime);
                    break;
        // Accumulate both dock idle time and dock idle hogged time
        case IDLE_HOGGED:   dock.idleHogTime += (globalTime - dock.prevTime);
                            dock.idleTime += (globalTime - dock.prevTime);
                            break;
    }
    // Set time marker
    dock.prevTime = globalTime;
}

// Updates the statistics structure 
void updateStat( Train *train )
{
    statistics.numOfTrainsServed++;
    statistics.sumOfTrainTimes+=(train->endDockTime - train->arrivalTime);
    statistics.maxOfTrainTimes=( (train->endDockTime - train->arrivalTime) > statistics.maxOfTrainTimes? (train->endDockTime - train->arrivalTime) : statistics.maxOfTrainTimes);
    statistics.sumOfInQTimes+=(train->endQTime - train->arrivalTime);
    statistics.maxTrainsInQ=( trainQ.size() > statistics.maxTrainsInQ? trainQ.size(): statistics.maxTrainsInQ );
   
    if( train->hogouts > 100 )
        return;
    for(int i = 0; i <= train->hogouts; ++i)
        statistics.histogram[i]++;
}

/* Action Functions */ 

// The event where a train arrives to the dock
// (Arrival time within train class is assumed to be absolute!)
void arrival( Train *currentTrain )
{
    // Print State
    cout << fixed << setprecision(2) << "Time " << globalTime <<": "
        << "train " << currentTrain->tid << " arrival for " << currentTrain->unloadTime 
        << "h of unloading, crew " << currentTrain->cid << " with "
        << currentTrain->timeTillHogout << "h before hogout (Q=" << trainQ.size() << ")\n";

    // If there is still simulation time or pre-made arrival times, schedule arrival
    if( (!premade && globalTime <= simTime) || (premade && !trainSchedule.empty()) )
    {   
        // Generate arrival, unload, hogout times
        TrainSchedule ts = getTrainValues();
        // Create a new train
        Train *trainPtr = new Train(nextTrainId++, nextCrewId++, ts.arrivalTime + globalTime, ts.remainingCrewHours, ts.unloadTime );
        // Schedule when the train will arrive
        schedule(arrival, trainPtr, ts.arrivalTime + globalTime);
    }
    // For the current train arrival, jump queue if it is empty and the dock is not busy
    if( trainQ.empty() && dock.dockStatus == IDLE )
    {
        startService( currentTrain );
    }
    else // Otherwise current train joins the queue
    {
        currentTrain->trainStatus = INQ;
        trainQ.push( currentTrain );
    }
    // Schedule when the trains crew will hogout 
    schedule(hogout, currentTrain, currentTrain->timeTillHogout + globalTime);
}

// The event where the train has finished unloading, and is leaving the dock. 
// Can only truly depart if train is not currently hogged out.
void departure( Train *train )
{
    // If the train is hogged out then it cannot depart.
    // There was another later departure time scheduled for the train when its crew hogged out
    if( train->crewStatus == HOGGED_OUT )
        return;
    // Print State
    cout << fixed << setprecision(2) << "Time " << globalTime << ": train " 
        << train->tid << " departing (Q=" << trainQ.size() << ")\n";
    // Train status is now departed, this is used by checkEvent to free up memory from the train
    train->trainStatus = DEPARTED;
    // Set what time the train left the dock
    train->endDockTime = globalTime;
    // Set the dock to not busy
    dock.dockStatus = IDLE;
    // Update the statistics
    updateStat( train );
    // Check to see if there is a train at the front of the queue that is not hogged and
    // start its service.
    if( !trainQ.empty() && trainQ.front()->crewStatus == ON_CLOCK )
    {
        Train *t = trainQ.front();
        trainQ.pop();
        startService( t );
    }
    else if( !trainQ.empty() )  // Otherwise the server is hogged and queue is blocked. 
                                // Dock will be unhogged by a new crew arrival event
    {
         cout << fixed << setprecision(2) << "Time " << globalTime <<": "
            << "train " << trainQ.front()->tid << " crew " << trainQ.front()->cid 
            << " hasn't arrived yet, cannot enter dock (SERVER HOGGED)\n";
    } 
}

// Event for when the trains crew has to hogout.
// Can either be done in the queue or while train is in dock.
void hogout( Train *train )
{
    // Set the trains crew status to hogged out
    train->crewStatus = HOGGED_OUT;
    // Increment the number of times this train has hogged out
    train->hogouts++;
    // Need to generate when a new crew will arrive
    double newCrewArrival = getCrewArrivalTime();
    // Calculate time remaining of the new crews shift once they have arrived
    train->timeTillHogout = (SHIFT_TIME - newCrewArrival);
    // If train was in the queue when its crew hogged out print relevant state
    if( train->trainStatus == INQ )
    {
        cout << fixed << setprecision(2) << "Time " << globalTime <<": "
            << "train " << train->tid << " crew " << train->cid 
            << " hogged out in queue\n";
    }
    else // Else train was in the dock if( train->trainStatus == SERVICE )
    {
        // There is a train at the dock but its crew has hogged out, set dock status.
        dock.dockStatus = IDLE_HOGGED;
        // Print relevant state 
        cout << fixed << setprecision(2) << "Time " << globalTime <<": "
            << "train " << train->tid << " crew " << train->cid 
            << " hogged out during service (SERVER HOGGED)\n";
        // The trains remaining unload time is updated based off elapsed time since it has been in the dock
        train->unloadTime -= (globalTime - train->endQTime);
    }
    // Reassign the trains crew id
    train->cid = nextCrewId++;
    // Schedule when the new train crew will arrive
    schedule( newCrew, train, globalTime + newCrewArrival );
}

// The event where the new crew has arrived to take over a train that has been hogged out
// It will schedule the next hogout for new crew only if train is in queue (when in service
// train will depart before hogging out)
void newCrew( Train *train )
{
    // Print state
    cout << fixed << setprecision(2) << "Time " << globalTime <<": "
        << "train " << train->tid << " replacement crew " << train->cid 
        << " arrives (SERVER UNHOGGED)\n";
    // Trains crew status changes from hogged to on the clock
    train->crewStatus = ON_CLOCK;
    // Set the time for when the train was put back into service
    train->crewArrivalTime = globalTime;
    // If this train was blocking the queue then it needs to be moved into dock,
    // if it is not busy.
    if( dock.dockStatus == IDLE && train->trainStatus == INQ && trainQ.front()->tid == train->tid )
    {
        Train *t = trainQ.front();
        trainQ.pop();
        startService( t );
    }
    else if( train->trainStatus == SERVICE ) // See if train was in the dock
    {
        // Dock status is no longer idle with train but is back to busy
        dock.dockStatus = BUSY;
        // This will update when the trains departure time (by sending new event)
        schedule( departure, train, globalTime+train->unloadTime );
    } // Otherwise train was somewhere in the queue behind another train
    // Schedule the new crews hogout time 
    // Don't schedule next hogout when train is in dock because train will depart by then
    if( train->trainStatus == INQ )
    {
        schedule( hogout, train, globalTime + train->timeTillHogout ); 
    }
}

// This implies the train in front of a nonempty queue was was also not hogged
// so it can be moved into dock to be unloaded and depart
void startService( Train *train )
{
    // Update the time till hogout
    train->timeTillHogout -= (globalTime - train->crewArrivalTime);
    // Print State
    cout << fixed << setprecision(2) << "Time " << globalTime <<": "
        << "train " << train->tid << " entering dock for " << train->unloadTime 
        << "h of unloading, crew " << train->cid << " with "
        << train->timeTillHogout << "h before hogout\n";

    // Train has finished its time in the queue
    train->endQTime = globalTime;
    // Train is now being unloaded
    train->trainStatus = SERVICE;
    // Dock is busy unloading this train
    dock.dockStatus = BUSY;
    // The train is scheduled to depart after train is done unloading 
    schedule( departure, train, train->unloadTime + globalTime );
}
