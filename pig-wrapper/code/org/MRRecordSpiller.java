package org;

import java.io.IOException;
import java.util.Iterator;

import org.apache.hadoop.io.Writable;
import org.apache.hadoop.io.WritableComparable;
import org.apache.hadoop.mapreduce.RecordWriter;
import org.apache.hadoop.mapreduce.TaskAttemptContext;
import org.apache.pig.backend.executionengine.ExecException;
import org.apache.pig.backend.hadoop.HDataType;
import org.apache.pig.data.BagFactory;
import org.apache.pig.data.DataBag;
import org.apache.pig.data.DataType;
import org.apache.pig.data.Tuple;
import org.apache.pig.data.TupleFactory;
import org.apache.pig.impl.io.NullableBag;
import org.apache.pig.impl.io.NullableTuple;
import org.apache.pig.impl.io.PigNullableWritable;

public class MRRecordSpiller extends org.apache.hadoop.mapreduce.RecordWriter<PigNullableWritable, Writable>
	implements MyComplexStream
{
	org.apache.hadoop.mapreduce.
		RecordWriter<PigNullableWritable, Writable> outputStream;
	
	BagFactory bagFactory;
	TupleFactory tupleFactory;
	
	final int SPILLABLE_LIMIT = 30 * 1000;
	//final int SPILLABLE_LIMIT = 0;
	
	MRRecordSpiller(org.apache.hadoop.mapreduce.
			RecordWriter<PigNullableWritable, Writable> outStream) 
	{
		outputStream = outStream;
		bagFactory = BagFactory.getInstance();
		tupleFactory = TupleFactory.getInstance();
	}

	@Override
	public void close(TaskAttemptContext ctx) throws IOException,
			InterruptedException 
	{
		outputStream.close(ctx);
	}
	
	void feedBag(PigNullableWritable key, NullableTuple value,
			DataBag bag, boolean is_nullable) throws IOException, InterruptedException 
	{
		Tuple result = tupleFactory.newTuple(1);
		
		if (is_nullable) {
			NullableBag b = (NullableBag) 
					HDataType.getWritableComparableTypes(bag, DataType.BAG);
			result.set(0, b);
		} else {
			result.set(0, bag);
		}
		
		outputStream.write(key, 
			MyUtil.cloneTupleOnlyAttributes(value, result));
	}
	
	void spillBag(PigNullableWritable key, NullableTuple value, 
			DataBag bag, boolean is_nullable) throws IOException, InterruptedException 
	{
		DataBag currBag = bagFactory.newDefaultBag();
		Iterator<Tuple> iter = bag.iterator();
		while (iter.hasNext()) {
			if (currBag.size() == SPILLABLE_LIMIT) {
				feedBag(key, value, currBag, is_nullable);
			}
			currBag.add(iter.next());
		}
		feedBag(key, value, currBag, is_nullable);
	}

	@Override
	public void write(PigNullableWritable key, Writable val)
			throws IOException, InterruptedException 
	{
		if (val instanceof NullableTuple && !((NullableTuple)val).isNull()) {
			Tuple ntuple = (Tuple) ((NullableTuple)val).getValueAsPigType();
			
			if (!(ntuple.get(0) instanceof NullableBag 
				&& (!((NullableBag)ntuple.get(0)).isNull())) 
				&& !(ntuple.get(0) instanceof DataBag)) 
			{
				outputStream.write(key, val);
				return;
			}
			
			boolean is_nullable = false;
			DataBag bag;
			if (ntuple.get(0) instanceof NullableBag) {
				bag = (DataBag) ((NullableBag)ntuple.get(0)).getValueAsPigType();
				is_nullable = true;
			} else {
				bag =  (DataBag) ntuple.get(0);
			}
			
			
			if (bag.size() > SPILLABLE_LIMIT) {
				spillBag(key, (NullableTuple)val, bag, is_nullable);
			} else {
				outputStream.write(key, val);
			}
			
			return;
		}
		
		outputStream.write(key, val);
	}

	@Override
	public RecordWriter<WritableComparable, Writable> getParentStream() {
		MyComplexStream outputStreamInterface = (MyComplexStream)outputStream;
		return outputStreamInterface.getParentStream();
	}

	 
	
}
