package org.apache.pig.impl.builtin;

import java.io.BufferedOutputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;

import org.MyComplexStream;
import org.MyRecordWriter;
import org.apache.hadoop.io.Writable;
import org.apache.hadoop.io.WritableComparable;
import org.apache.hadoop.mapred.JobConf;
import org.apache.hadoop.mapreduce.JobContext;
import org.apache.hadoop.mapreduce.OutputCommitter;
import org.apache.hadoop.mapreduce.RecordWriter;
import org.apache.hadoop.mapreduce.TaskAttemptContext;
import org.apache.pig.builtin.PigStorage;
import org.apache.pig.data.Tuple;
import org.apache.pig.impl.io.PigNullableWritable;

class MyOutputFormat extends org.apache.hadoop.mapreduce.OutputFormat {

	@Override
	public OutputCommitter getOutputCommitter(TaskAttemptContext arg0)
			throws IOException, InterruptedException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public RecordWriter getRecordWriter(TaskAttemptContext arg0)
			throws IOException, InterruptedException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public void checkOutputSpecs(JobContext arg0) throws IOException,
			InterruptedException {
		// TODO Auto-generated method stub
		
	}
	
}



public class MyPigStorage extends PigStorage {
	String fClassName;
	String fPathName;
	org.MyRecordWriter writer;
	
	public MyPigStorage(String [] args) {
		super(args[2]);
		fClassName = args[0];
		fPathName = args[1];
		writer = (org.MyRecordWriter) funcMap.get(this.fPathName);
	}
	
	public void putNext(Tuple t) throws IOException {
		try {
			writer.write(null, (Writable)t);
		} catch (InterruptedException e) {
			System.err.println("write failed");
		}
	}
	
	@Override
	public org.apache.hadoop.mapreduce.OutputFormat getOutputFormat() {
		return new MyOutputFormat();
	}

	public static void resolveFuncMap() throws InterruptedException, IOException {
		String [] keys = funcMap.keySet().toArray(new String[0]);
		Arrays.sort(keys);
		
		for (int i = 0; i < keys.length; ++i) {
			MyComplexStream writerInterface = (MyComplexStream)funcMap.get(keys[i]);
			org.MyRecordWriter writer = (org.MyRecordWriter) writerInterface.getParentStream();
			 
			OutputStream outStream = new BufferedOutputStream(
				new FileOutputStream("/proc/self/fd/" + (2 * i + 3 + 1)));

			writer.setOutputStream(outStream);
			writer.initOutputImpl(func2Class.get(keys[i]), func2Args.get(keys[i]));
			
			if (i == 0) {
				firstTableWriter = writer;
			}
		}
	}
	
	public static void insertWriter(String key,
		RecordWriter writer, String cl, String args) 
	{
		funcMap.put(key, writer);
		func2Class.put(key, cl);
		func2Args.put(key, args);
	}
	
	public static Map<String, RecordWriter> funcMap
		= new HashMap<String, RecordWriter>();
	public static Map<String, String> func2Class
	= new HashMap<String, String>();
	public static Map<String, String> func2Args
	= new HashMap<String, String>();
	public static RecordWriter firstTableWriter = null;
	
	public static void closeWriters() throws IOException, InterruptedException {
		for (RecordWriter writer: funcMap.values()) {
			writer.close(null);
		}
	}
}

