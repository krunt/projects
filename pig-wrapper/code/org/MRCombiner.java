package org;

import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map.Entry;

import org.apache.hadoop.io.Writable;
import org.apache.hadoop.io.WritableComparable;
import org.apache.hadoop.mapred.JobConf;
import org.apache.hadoop.mapred.TaskAttemptID;
import org.apache.hadoop.mapreduce.RecordWriter;
import org.apache.hadoop.mapreduce.Reducer;
import org.apache.hadoop.mapreduce.TaskAttemptContext;
import org.apache.hadoop.mapreduce.Reducer.Context;
import org.apache.hadoop.util.ReflectionUtils;
import org.apache.pig.impl.builtin.MyPigStorage;
import org.apache.pig.impl.io.NullableTuple;
import org.apache.pig.impl.io.PigNullableWritable;


public class MRCombiner extends org.apache.hadoop.mapreduce.RecordWriter<PigNullableWritable, Writable>
	implements MyComplexStream
{
	JobConf conf;
	org.apache.hadoop.mapreduce.
		RecordWriter<PigNullableWritable, Writable> outputStream;
	HashMap<PigNullableWritable, ArrayList<Writable>> combineStorage;
	org.apache.hadoop.mapreduce.Reducer<PigNullableWritable, Writable, PigNullableWritable, Writable>
		combiner;
	
	static final int ITEMS_MAX = 100000; 
	
	MRCombiner(JobConf conf, org.apache.hadoop.mapreduce.
		RecordWriter<PigNullableWritable, Writable> outStream)
	{
		outputStream = outStream;
		this.conf = conf;
		combineStorage = new HashMap<PigNullableWritable, ArrayList<Writable>>();
		
		combiner = (Reducer<PigNullableWritable, Writable, PigNullableWritable, Writable>)
			ReflectionUtils.newInstance(conf.getClass("mapreduce.combine.class", 
					org.apache.hadoop.mapreduce.Reducer.class), conf);
	}

	@Override
	public RecordWriter<WritableComparable, Writable> getParentStream() {
		MyComplexStream outputStreamInterface = (MyComplexStream)outputStream;
		return outputStreamInterface.getParentStream();		
	}

	@Override
	public void close(TaskAttemptContext arg0) throws IOException,
			InterruptedException 
	{	
		if (combineStorage.size() > 0) {
			flushCombineStorage();
		}
		
		outputStream.close(arg0);
	}
	
	void flushCombineStorage() throws IOException, InterruptedException {
		final Iterator<Entry<PigNullableWritable, ArrayList<Writable>>> 
			outIter = combineStorage.entrySet().iterator();			
			
			Context combineContext
			= combiner.new Context(
					conf, 
					new TaskAttemptID(),
					new StubRawKeyValueIterator(),
					null,
					null,
					outputStream,
					null, 
					new MyStatusReporter(),
					null, 
					PigNullableWritable.class, Writable.class)
			{
				Entry<PigNullableWritable, ArrayList<Writable>> currEntry = null;
				
				public boolean nextKey() throws IOException,InterruptedException {
					if (!outIter.hasNext()) {
						return false;
					}
					currEntry = outIter.next();
					return true;
				  }

				  /**
				   * Advance to the next key/value pair.
				   */
				  @Override
				  public boolean nextKeyValue() throws IOException, InterruptedException {				  
					throw new RuntimeException("not implemented");
				  }

				  public PigNullableWritable getCurrentKey() {
				    return currEntry.getKey();
				  }

				  @Override
				  public NullableTuple getCurrentValue() {
					throw new RuntimeException("not implemented");
				  }
				  
				  class ValueIterable implements Iterable<Writable> {
					  Iterator<Writable> iterator;
					  
					  ValueIterable(Iterator<Writable> iterator2) {
						  iterator = iterator2;
					  }
					  
					  public Iterator<Writable> iterator() {
						  return iterator;
					  }
				  }

				  public 
				  Iterable<Writable> getValues() throws IOException, InterruptedException {
				    return new ValueIterable(currEntry.getValue().iterator());
				  }
			};
			
			combiner.run(combineContext);
			
			combineStorage.clear();
		}

	@Override
	public void write(PigNullableWritable arg0, Writable arg1)
			throws IOException, InterruptedException 
	{
		if (!combineStorage.containsKey(arg0)) {
			combineStorage.put(arg0, new ArrayList<Writable>());
		}
		combineStorage.get(arg0).add(arg1);
		
		if (combineStorage.size() > ITEMS_MAX) {
			flushCombineStorage();
		}
	}

}

