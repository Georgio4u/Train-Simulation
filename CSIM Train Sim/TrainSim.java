package com.trainsim;

import com.mesquite.csim.*;
import com.mesquite.csim.Process;
import com.mesquite.csim.file.Files;
import java.lang.*;
import java.io.*;

// Train Simulation class
public class TrainSim extends Model {
    public static void main(String[] args) {
        // Check argument count to decide how to execute simulation
        if( args.length == 3 && args[0].equals( "-s" ) ) {
            // Using pre-defined values
            premade = true;
            schedFile = args[1];
            travelFile = args[2];
        }
        else if( args.length == 3 ) {
            // Using generated values and running n sims
            interATime = Double.parseDouble( args[0] );
            simTime = Double.parseDouble( args[1] );
            nSims = Integer.parseInt( args[2] );
        }
        else if( args.length == 2 ) {
            // Using generated values running sim once
            interATime = Double.parseDouble( args[0] );
            simTime = Double.parseDouble( args[1] );
        }
        // Create a new instance of this class to continue the simulation
        // and run it
        TrainSim model = new TrainSim();
        model.run();
        // Print a report only if more than 1 simulation are to be run
        if( nSims > 1 )
            model.report();
    }

    public TrainSim() {
        super( "Train Unloading Dock Simulation" );
    }

    public void run() {
        try {
            // Create N simulations
            for( int i = 0; i < nSims; ++i ) {
                start(new Sim());
            }
        }
        catch (Exception e) {
            System.out.println("csim error: " + e.getMessage());
            System.exit(0);
        }
    }

    private static double simTime = 72000.0; // Max simulation time, default is 72000
    private static double interATime = 10.0; // train inter-arrive rate 
    private static int simCount = 0; // Meant for sim id assignment  
    private static int nSims = 1; // N simulations
    private static boolean endSim = false; // Flag used to stop simulation
    private static boolean premade = false; // Flag used to decide how we are getting values
    private static String schedFile; // Schedule of trains file
    private static String travelFile; // crew travel times file
    private FCFSFacility dock; // The unloading dock
    private Gen gen; // Class used to generate each train arrival
    private TrainSimUtil util; // Used to generate or grab next train/crew value
    private LastTrain lastTrain; // Last train event used to end a sim
    enum CrewStat { ONCLOCK, HOGGED }; // Crew statuses 
    enum TrainStat { INQ, INDOCK }; // Train statuses 
    private Table timeInTable; // Table of per-train time in system times
    private Table idleTable; // Table of dock idle times (per sim)
    private Table busyTable; // Table of dock busy times (per sim)
    private Table hoggedTable; // Table of crew hogged in dock time (per sim)
    private Table timeInQ; // Table of per-train times spent in queue
    private static int maxQ; // The max train queue size (per sim)
    private int[] histogram; // Train hogout histogram (per sim)
    private Table conf;  // Table of per-train means of each sim

    // Simulation class used to simulate the unloading dock
    private class Sim extends Process {
        public Sim() {
            super( "Sim " + Integer.toString(simCount++) );
            if( premade ) {
                try{
                    // Create a new utility object with predefined values
                    util = new TrainSimUtil( schedFile, travelFile );
                } catch( FileNotFoundException e ) { 
                    System.out.println( "Could not find files" );
                    System.exit(0);
                } catch( IOException e ) {
                    System.out.println( "Could not open files" );
                    System.exit(0);
                }
            }
            else { 
                // Create a new utility object that generates values
                util = new TrainSimUtil( interATime, 3.5, 4.5, 6.0, 11.0, 2.5, 3.5 ); 
            }
        }
        public void run() {
            // If the confidence interval table already exist don't recreate it
            if( conf == null ) {
                conf = new Table( "Confidence interval of the means of in system time" );
                conf.run_length(.01, .99, 10000 );
                conf.setPermanent( true );
            }
            // Set initializations
            endSim = false;
            dock = new FCFSFacility("Unloading Dock", 1);
            gen = new Gen();
            /* table initializations */
            timeInTable = new Table( "Time in system" );
            idleTable = new Table( "Dock idle table" );
            busyTable = new Table( "Dock busy table" );
            hoggedTable = new Table( "Dock Hogged-out table" );
            timeInQ = new Table( "Time in queue" );
            /* end table initializations */
            // initialize the histogram to 0 
            histogram = new int[100];
            for( int i = 0; i < 100; ++i)
                histogram[i] = 0;
            maxQ = 0;
            // Create the last train Event
            lastTrain = new LastTrain();
            // start generating train arrivals 
            add( gen );
            // wait till last train arrival (based on sim time)
            lastTrain.untimed_wait();
            // Fixes the error of not waiting for last departure 
            hold( 10.0 );
            // Record the simulations time-in-system mean
            conf.record( timeInTable.mean() );
            // Establish the time the this sim ended at
            System.out.printf( "Time %.2f: simulation ended\n", clock() );
            // Print relative statistics
            printStats();
            try {
                // Closes files if needed to
                util.finalize();
            } catch( IOException e ) {
                System.out.println( "Error closing files" );
                System.exit(0);
            }
        }

        // Simulation statistics
        public void printStats() {
            FacilityStats stats = dock.stats();
            System.out.println("");
            System.out.println( "Statistics" );
            System.out.println( "----------" );
            System.out.printf( "Total number of trains served: %d\n", stats.completions() ); 
            System.out.printf( "Average time-in-system per train: %.2fh\n", timeInTable.mean() );
            System.out.printf( "Maximum time-in-system per train: %.2fh\n", timeInTable.max() );
            System.out.printf( "Dock idle percentage: %.2f%%\n", idleTable.sum() / clock() * 100);
            System.out.printf( "Dock busy percentage: %.2f%%\n", busyTable.sum() / clock() * 100);
            System.out.printf( "Dock hogged-out percentage: %.2f%%\n", hoggedTable.sum() / clock() * 100);
            System.out.printf( "Time average of trains in queue: %.3f\n", timeInQ.sum() / stats.completions() );
            System.out.printf( "Maximum number of trains in queue: %d\n", maxQ );
            System.out.println( "Histogram of hogout count per train:" );
            printHisto();
            conf.confidence(); // We want confidence interval in the report 
        }

        // Histogram to print    
        public void printHisto() {
            for( int i = 0; i < 100; ++i ) {
                if( histogram[i] != 0 ) { // Don't print zero values
                    System.out.printf( "[%d]: %d\n", i, histogram[i] );
                }
            }
        } 

    }

    // Generate train arrivals class
    private class Gen extends Process {
        private int trainId; // Next Train id
        private int crewId; // Next Crew id 
        public Gen() {
            super( "Gen" );
            trainId = 0;
            crewId = 0;
        }
        public void run() {
            while( true ) {
                // Create a new train
                Train t = new Train();
                // If train can't be made simTime is up or file is empty
                if (endSim) break;
                // Hold inter arrival time amount till next train
                hold( t.iarTime );
                // Start the trains process
                add( t );
            }
        }
        public String getTrainId() {
            return Integer.toString( trainId++ );
        }
        public String getCrewId() {
            return Integer.toString( crewId++ );
        }
    }
    
    // Train class
    private class Train extends Process {
        private double startTime; // Time the train starts in the system
        private double endQTime; // Time the train left the queue
        private double iarTime; // This trains inter-arrival time
        private double unloadTime; // Time it takes to unload the train
        private double cClock; // Crews remaining hours 
        private TrainStat status; // Current status of the train
        private Crew crew; // The connected crew process
        private Event eHogout; // hogout event
        private Event eNewCrew; // new crew arrival event
        public Train() {
            super( gen.getTrainId() );
            try{
                // Used to hold relevant train values (generated or ungenerated)
                String[] tokens = util.getNextTrainValues();
                // If Sim time is up or schedule file is empty, signal for endSim
                if( tokens == null || simTime < clock() ) {
                    endSim = true;
                    return;
                }
                // Used to adjust for the schedule file having exact arrival times
                // and not inter-arrival times
                if( premade )
                    iarTime = Double.parseDouble( tokens[0] ) - clock();
                else 
                    iarTime = Double.parseDouble( tokens[0] ); 
                unloadTime = Double.parseDouble( tokens[1] );
                /* create events */
                eHogout = new Hogout();
                eNewCrew = new NewCrew();
                /* end */ 
                cClock = Double.parseDouble( tokens[2] );
                startTime = 0.0;
                endQTime = 0.0;
            } catch( IOException e ) { 
                System.out.println( "Error reading from schedule file" ); 
                System.exit(0);
            }
        }

        public void run() {
            boolean jump = false; // Used to not get stuck in an immediate next hogout
            double dockIdle = 0.0; // Used for dock idle stats
            double dockHog = 0.0; // Used for dock hogged stats
            double start = 0.0; // Used to mark a start point to record from
            // Busy time is just unload time
            busyTable.record(unloadTime);
            // * TRAIN ARRIVES */
            startTime = clock();
            // Crew times start
            crew = new Crew( this, cClock );
            add( crew );
            printArrival();
            /* TRAIN ENTER QUEUE */
            // Set trains status to in queue
            status = TrainStat.INQ;
            // Train waits for its turn in the dock
            dock.reserve();
            // Update time till crew hogs out
            crew.updateTimeLeft();
            /* MOVES INTO DOCK IF NOT HOGGED */
            // Move train into dock if crew isn't hogged out else train is stuck
            // in queue and has to wait for a new crew
            if( crew.status == CrewStat.HOGGED ) {
                start = clock();// mark start
                printStuckInQ();
                eNewCrew.untimed_wait();// wait for the new crew
                jump = true; // Used to not get stuck in an immediate next hogout
                dockIdle += (clock() - start); // stat taking 
            }
            endQTime = clock();
            // Train is in dock and can start unloading. 
            crew.updateTimeLeft();
            /* IN DOCk */
            printEnterDock();
            status = TrainStat.INDOCK;
            // If crew hogs out during unloading, stop unloading and wait for a new crew.
            // After new crew arrives continue the remaining unloading
            if( !jump && eHogout.timed_wait( unloadTime ) ) {
                start = clock(); // mark start
                unloadTime -= (clock() - endQTime); // update unloading time to remaining
                eNewCrew.untimed_wait(); // wait for new crew
                dockIdle += (clock() - start); // stat
                dockHog += (clock() - start); // stat
                hold( unloadTime ); // Wait the remaining unloading time
            }
            /* DEPARTING */
            // Train is unloaded and departing from the dock. Crew process stopped.
            crew.depart();
            printDeparture();
            checkMaxQ(); // Check for new max queue size
            dock.release();
            updateHisto(); // update histogram
            // Time in the system
            timeInTable.record(clock() - startTime);
            // Time in queue
            timeInQ.record( endQTime - startTime );
            // Dock idle time
            idleTable.record(dockIdle);
            // Dock hogged-out time
            hoggedTable.record(dockHog);
            // Send out last train event when end sim flag is raised
            if( endSim ) {
                lastTrain.set();
            }
        }

        /* PRINT FUNCTIONS */
        public void printArrival() {
            System.out.printf("Time %.2f: train %s arrival for %.2fh of unloading, " +
                               "crew %s with %.2fh before hogout (Q=%d)\n",
                               model.clock(), this.name(), this.unloadTime,
                               crew.id, crew.timeLeft, dock.qlength());
        }
        public void printEnterDock() {
            System.out.printf("Time %.2f: train %s entering dock for %.2fh of "
                + "unloading, crew %s with %.2fh before hogout\n",
                               model.clock(), this.name(), this.unloadTime,
                               crew.id, crew.timeLeft);
        }
        public void printDeparture() {
            System.out.printf("Time %.2f: train %s departing (Q=%d)\n",
                               model.clock(), this.name(), dock.qlength());
        }
        public void printHogoutInQ() {
            System.out.printf("Time %.2f: train %s crew %s hogged out in queue\n",
                               model.clock(), this.name(), crew.id);
        }
        public void printHogoutInDock() {
            System.out.printf("Time %.2f: train %s crew %s hogged out during " 
                              + "service (SERVER HOGGED)\n",
                               model.clock(), this.name(), crew.id);   
        }
        public void printCrewArrival() {
             System.out.printf("Time %.2f: train %s replacement crew %s arrives " 
                              + "(SERVER UNHOGGED)\n",
                               model.clock(), this.name(), crew.id); 
        }
        public void printStuckInQ() {
             System.out.printf("Time %.2f: train %s crew %s hasn't arrived yet, " 
                              + "cannot enter dock (SERVER HOGGED)\n",
                               model.clock(), this.name(), crew.id); 
        }
        /* END */

        // Will compare current max to queue size, update if needed
        public void checkMaxQ() {
            if( dock.qlength() > maxQ )
                maxQ = dock.qlength();
        }
        
        // Updates the hogout histogram
        public void updateHisto() {
            for( int i = 0; i <= crew.hogoutCount; ++i )
                histogram[i]++;
        }

    }

    // The crew class process
    private class Crew extends Process {
        Train train; // Reference to its train
        private String id; // Crew id
        private CrewStat status; // Crew status
        private double timeLeft; // Time till crew hogs out
        private double crewArrivalTime; // The time the new crew arrived at
        private int hogoutCount; // count how many times this train had a crew hogout
        private boolean departed; // If the crew has departed or not
        public Crew( Train t, Double tLeft ) {
            super( "Crew" );
            train = t;
            status = CrewStat.ONCLOCK;
            timeLeft = tLeft;
            hogoutCount = 0;
            departed = false;
            id = gen.getCrewId();
        }

        public void run() {
            crewArrivalTime = train.startTime;
            while( true ) {
                // Wait till crew has to hogout
                hold( timeLeft );
                // if train departed then don't worry about hogging out
                if( departed ) break;
                // If train is in the queue print those details
                if( train.status == TrainStat.INQ ) {
                    train.printHogoutInQ();
                }
                // If train is in dock print those details
                if ( train.status == TrainStat.INDOCK ) {
                    train.printHogoutInDock();
                }
                try {
                    // Get new crew arrival time
                    double timeTillAr = util.getNextCrewArrival();
                    // Adjust remaining hours from arrival time
                    timeLeft = (12.0 - timeTillAr);
                    // Set the status of the crew and update info
                    status = CrewStat.HOGGED;
                    hogoutCount++;
                    id = gen.getCrewId();
                    train.eHogout.set();
                    /* WAIT FOR NEW CREWW */
                    hold( timeTillAr );
                    train.printCrewArrival();
                    crewArrivalTime = clock();
                    status = CrewStat.ONCLOCK;
                    // Set the event 
                    train.eNewCrew.set();
                } catch( IOException e) { 
                    System.out.println( "Error reading from travel times file" ); 
                    System.exit(0);
                }
            }
        }

        public void depart() {
            departed = true;
        }

        public void updateTimeLeft() {
            //System.out.println( timeLeft - (clock() - crewArrivalTime) ) ;
        }
    }

    /* EVENTS */
    private class Hogout extends Event {
        public Hogout() {
            super("Crew Hogged Out");
        }
    }

    private class NewCrew extends Event {
        public NewCrew() {
            super("New Crew Arrived");
        }
    }

    private class LastTrain extends Event {
        public LastTrain() {
            super("Last Train Event");
        }
    }
    /* END */

    // Used to get all the different times needed for the sim
    private class TrainSimUtil {
        boolean generateValues; // Flag for generating or reading values
        double arrRate = 10.0; // Train arrival rate
        double unTimeA = 3.5; // Unload rate start range
        double unTimeB = 4.5; // Unload rate end range
        double crewWorkTimeA = 6.0; // Crews time left start range;
        double crewWorkTimeB = 11.0; // Crews time left end range;
        double replaceTimeA = 2.5; // Replacement crews arrival start range;
        double replaceTimeB = 3.5; // Replacement crews arrival end range;
        BufferedReader brSchedule = null; // Used to read train schedules file
        BufferedReader brTravel = null; // Used to read crew arrival time schedules
        Random randArr = new Random(); // Random number generator for train arrival times
        Random randUn = new Random(); // Random number generator for train unload times
        Random randRemain = new Random();// Random number generator for remaining crew times
        Random randReplace = new Random(); // Random number generator for replacement crew arrival times
        /*
        Constructor for randomly generated values
        Parameter(1): Train arrival rate
        Parameter(2): Total simulation hours
        */
        TrainSimUtil( double arrival, double unloadA, double unloadB, 
            double crewA, double crewB, double replaceA, double replaceB) {
            // Assign values
            generateValues = true;
            arrRate = arrival;
            unTimeA = unloadA;
            unTimeB = unloadB;
            crewWorkTimeA = crewA;
            crewWorkTimeB = crewB;
            replaceTimeA = replaceA;
            replaceTimeB = replaceB;
        }
        
        /*
        Constructor for pre-generated values
        Parameter(1): File location to train arrival schedule, unloading time, and
        remaining crew hours
        Parameter(2): File location to train
        */
        TrainSimUtil( String schedule, String travelTimes ) throws FileNotFoundException, IOException {
            generateValues = false;
            // Open files for reading pre-made values
            brSchedule = new BufferedReader( new FileReader(schedule) );
            brTravel = new BufferedReader( new FileReader(travelTimes) );
        }
        // 'Destructor' used to close open files
        public void finalize() throws IOException { 
            if( brSchedule != null )
                brSchedule.close();
            if( brTravel != null )
                brTravel.close();
        }
        // Return train arrival, unloading, and crews remaining hours
        // Randomly generated values or pre-made values depend on flag
        // Return null if the file is empty
        public String[] getNextTrainValues() throws IOException {
            if( !generateValues ) {
                String line;
                if( (line = brSchedule.readLine()) != null ) {
                    String[] tokens = line.split(" ");
                    return tokens;
                } else {
                    return null;
                }
            } else {
                String[] tokens = new String[3];
                tokens[0] = Double.toString( randArr.exponential(arrRate) );
                tokens[1] = Double.toString( randUn.uniform(unTimeA, unTimeB ) );
                tokens[2] = Double.toString( randRemain.uniform( crewWorkTimeA, crewWorkTimeB ) );
                return tokens;
            }
        }

        // Return the new crews arrival times
        // Randomly generated values or pre-made values depend on flag
        // Return a value < 0 if the file is empty
        public double getNextCrewArrival() throws IOException {
            if( !generateValues ) {
                String line;
                if( (line = brTravel.readLine()) != null ) {
                    return Double.parseDouble( line );
                } else {
                    return -1.0;
                }
            } else {
                return randReplace.uniform( replaceTimeA, replaceTimeB );
            }
        }
    }
}
