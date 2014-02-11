package org;

import java.io.IOException;
import java.util.*;
import java.lang.*;

import org.apache.hadoop.fs.Path;
import org.apache.hadoop.conf.*;
import org.apache.hadoop.mapred.*;
import org.apache.hadoop.io.*;
import org.apache.hadoop.util.*;

class StdoutOutputCollector implements OutputCollector<Text, Text> {
	public final String OUTPUT_SEPARATOR = "\t";
	
	public void collect(Text key, Text value) {
		System.out.print(key);
		System.out.print(OUTPUT_SEPARATOR);
		System.out.print(value);
		System.out.println();
	}
}

class MyReduceRunner<K2, V2, K3, V3> {
	private K2 prevKey = null;
	private Reducer<K2, V2, K3, V3> reducer; 
	
	MyReduceRunner(JobConf conf, boolean combine) {
		this.reducer = ReflectionUtils.newInstance(
			combine ? conf.getCombinerClass() : conf.getReducerClass(), conf);
	}
	
	public void run(RecordReader<K2, V2> in, OutputCollector<K3, V3> output, Reporter reporter) throws Exception {
		try {
			K2 k = in.createKey();
			V2 v = in.createValue();
			Vector<V2> values = new Vector<V2>(3);
			
			while (in.next(k, v)) {
				if (prevKey != null && !prevKey.equals(k)) {
					reducer.reduce(k, values.iterator(), output, reporter);
					values.clear();
				}
				
				values.add(v);
				prevKey = k;
			}
			
			if (prevKey != null) {
				reducer.reduce(k, values.iterator(), output, reporter);
				values.clear();
			}
			
		} finally {
			reducer.close();
		}
	}
}

public class MapWrapper {
	
	static final int MAX_LINE_LENGTH = 4096;
	
	/* args[0] - path to submitted job.xml */
	public static void main(String[] args) throws Exception {
		if (args.length != 2) {
			throw new RuntimeException("usage: job.xml map|reduce|combine");
		}
		
		JobConf conf = new JobConf(new Path(args[0]));
		
		MyKeyValueLineRecordReader inputStream 
			= new MyKeyValueLineRecordReader(conf, System.in, MAX_LINE_LENGTH);
		StdoutOutputCollector outputStream = new StdoutOutputCollector();
	
		if (args[1].equals("map")) {
			MapRunner<Text, Text, Text, Text> mapper
				= new MapRunner<Text, Text, Text, Text>(); 
			mapper.configure(conf);
		
			mapper.run(inputStream, outputStream, Reporter.NULL);
			
		} else if (args[1].equals("reduce")) {
			MyReduceRunner<Text, Text, Text, Text> reducer
				= new MyReduceRunner<Text, Text, Text, Text>(conf, false);
			reducer.run(inputStream, outputStream, Reporter.NULL);
			
		} else if (args[1].equals("combine")) {
			MyReduceRunner<Text, Text, Text, Text> reducer
				= new MyReduceRunner<Text, Text, Text, Text>(conf, true);
			reducer.run(inputStream, outputStream, Reporter.NULL);			
			
		} else {
			throw new RuntimeException("args[1] must be map, reduce or combiner");
		}
	}
}

