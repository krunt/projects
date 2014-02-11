package org;

import org.apache.hadoop.mapred.Counters;
import org.apache.hadoop.mapreduce.Counter;
import org.apache.hadoop.mapreduce.StatusReporter;

class MyStatusReporter extends StatusReporter {
    Counters cnts = new Counters();
    
    @Override
    public Counter getCounter(Enum<?> arg0) {
        // TODO Auto-generated method stub
        return cnts.getGroup("main").getCounterForName("mystatus1");
    }   

    @Override
    public Counter getCounter(String arg0, String arg1) {
        // TODO Auto-generated method stub
        return cnts.getGroup("main").getCounterForName("mystatus1");
    }   

    @Override
    public float getProgress() {
        return 1;
    }   

    @Override
    public void progress() {
    }   

    @Override
    public void setStatus(String arg0) {
    }   
    
}