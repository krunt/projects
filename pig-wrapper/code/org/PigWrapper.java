package org;

import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.Serializable;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.*;
import java.lang.*;



import org.apache.commons.codec.binary.Base64;
import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.conf.*;
import org.apache.hadoop.mapred.*;
import org.apache.hadoop.mapreduce.Counter;
import org.apache.hadoop.mapreduce.Job;
import org.apache.hadoop.mapreduce.JobContext;
import org.apache.hadoop.mapreduce.OutputCommitter;
import org.apache.hadoop.mapreduce.RecordReader;
import org.apache.hadoop.mapreduce.RecordWriter;
import org.apache.hadoop.mapreduce.StatusReporter;
import org.apache.hadoop.mapreduce.TaskAttemptContext;
import org.apache.hadoop.mapreduce.lib.partition.HashPartitioner;
import org.apache.hadoop.mapreduce.split.JobSplit;
import org.apache.hadoop.mapreduce.split.SplitMetaInfoReader;
import org.apache.hadoop.io.*;
import org.apache.hadoop.io.serializer.Deserializer;
import org.apache.hadoop.io.serializer.SerializationFactory;
import org.apache.hadoop.io.serializer.Serializer;
import org.apache.hadoop.util.*;
import org.apache.hadoop.util.hash.Hash;
import org.apache.pig.CollectableLoadFunc;
import org.apache.pig.FuncSpec;
import org.apache.pig.IndexableLoadFunc;
import org.apache.pig.LoadFunc;
import org.apache.pig.OrderedLoadFunc;
import org.apache.pig.PigException;
import org.apache.pig.backend.executionengine.ExecException;
import org.apache.pig.backend.hadoop.HDataType;
import org.apache.pig.backend.hadoop.executionengine.mapReduceLayer.JobControlCompiler;
import org.apache.pig.backend.hadoop.executionengine.mapReduceLayer.MergeJoinIndexer;
import org.apache.pig.backend.hadoop.executionengine.mapReduceLayer.PigInputFormat;
import org.apache.pig.backend.hadoop.executionengine.mapReduceLayer.PigSplit;
import org.apache.pig.backend.hadoop.executionengine.physicalLayer.plans.PhysicalPlan;
import org.apache.pig.backend.hadoop.executionengine.physicalLayer.relationalOperators.POLoad;
import org.apache.pig.backend.hadoop.executionengine.physicalLayer.relationalOperators.POStore;
import org.apache.pig.backend.hadoop.executionengine.physicalLayer.util.PlanHelper;
import org.apache.pig.backend.hadoop.executionengine.shims.HadoopShims;
import org.apache.pig.backend.hadoop.executionengine.util.MapRedUtil;
import org.apache.pig.builtin.PigStorage;
import org.apache.pig.data.DataType;
import org.apache.pig.data.SortedDataBag;
import org.apache.pig.data.Tuple;
import org.apache.pig.data.TupleFactory;
import org.apache.pig.impl.PigContext;
import org.apache.pig.impl.builtin.MyPigStorage;
import org.apache.pig.impl.io.FileSpec;
import org.apache.pig.impl.io.NullableTuple;
import org.apache.pig.impl.io.PigNullableWritable;
import org.apache.pig.impl.plan.OperatorKey;
import org.apache.pig.impl.util.ObjectSerializer;
import org.apache.pig.impl.util.Pair;

import ru.yandex.statbox.MultiFormatParser;


class MyKeyValueRecordReader 
	extends org.apache.hadoop.mapreduce.RecordReader<Text, Tuple> 
{	
	static final int MAX_LINE_LENGTH = 64 << 20;
	
	JobConf conf = null;
	
	MyKeyValueLineRecordReader inputStream;
	Text currentKey = null;
	Text currentTextValue = null;
	protected Tuple currentValue = TupleFactory.getInstance().newTuple();
	
	MyBase64Deserializer<Tuple> tupleDeserializer;
	
	MyKeyValueRecordReader(JobConf conf, InputStream stream,
			String filterPattern) throws IOException 
	{
		this.conf = conf;
		
		inputStream = filterPattern != null 
			? new MyKeyValueLineRecordReaderFiltered(conf, 
					stream, MAX_LINE_LENGTH, filterPattern)
			: new MyKeyValueLineRecordReader(conf, stream, MAX_LINE_LENGTH);
		currentKey = inputStream.createKey();
		currentTextValue = inputStream.createValue();
		
		tupleDeserializer = new MyBase64Deserializer<Tuple>(conf, Tuple.class);
	}
	
	protected void convertTextValueToTupleValue(Text key, Text value) throws IOException {
		currentValue = tupleDeserializer.deserialize(value.getBytes(), 
				0, value.getLength(), currentValue);
	}
	
	public boolean nextKeyValue() throws IOException, InterruptedException {
		boolean result = inputStream.next(currentKey, currentTextValue);

		if (result) {
			convertTextValueToTupleValue(currentKey, currentTextValue);
		}
		
		return result;
	}
	
	public Text getCurrentKey() throws IOException, InterruptedException {
		//System.err.println(currentKey.toString());
		return currentKey;
	}
	
	public Tuple getCurrentValue() throws IOException, InterruptedException {
		//System.err.println(currentValue.toString());
		return currentValue;
	}	
	
	public float getProgress() throws IOException, InterruptedException {
		return 1;
	}
	
	public void close() throws IOException {
		if (inputStream != null) {
			inputStream.close();
		}
	}

	@Override
	public void initialize(org.apache.hadoop.mapreduce.InputSplit arg0,
			org.apache.hadoop.mapreduce.TaskAttemptContext arg1)
			throws IOException, InterruptedException 
	{
	}
}

//TODO: unquote tsv if needed
class TSVReadRecordStream extends MyKeyValueRecordReader {
	
	TSVReadRecordStream(JobConf conf, InputStream stream, String filterPattern) throws IOException {
		super(conf, stream, filterPattern);
	}
	
	protected void convertTextValueToTupleValue(Text key, Text value) throws IOException {
		ArrayList<Integer> offsets = new ArrayList<Integer>();
		
		int offset = 0;
		while (offset != -1) {
			int nextOffset = MyKeyValueLineRecordReader.findSeparator(value.getBytes(), 
				offset, value.getLength() - offset, (byte)'\t');
			
			int offsetToStore = nextOffset != -1 ? nextOffset : value.getLength();
			offsets.add(new Integer(offsetToStore));
			
			offset = nextOffset == -1 ? -1 : nextOffset + 1;	
		}
		
		currentValue = TupleFactory.getInstance().newTuple(offsets.size());
		
		offset = 0;
		for (int i = 0; i < offsets.size(); ++i) {
			currentValue.set(i, new String(value.getBytes(), offset, 
				offsets.get(i) - offset));
			offset = offsets.get(i) + 1;
		}
	}
	
}

/*
class TSKVReadRecordStream extends MyKeyValueRecordReader {
	
	MultiFormatParser tskvParser;
	Map<String, String> parsedRec;
	
	TSKVReadRecordStream(JobConf conf, InputStream stream, String filterPattern) throws IOException {
		super(conf, stream, filterPattern);
		tskvParser = new MultiFormatParser("/usr/share/statbox-class-filestorage/LogParser-Multiformat");
	}
	
	protected void convertTextValueToTupleValue(Text key, Text value) throws IOException {
	
		try {
			parsedRec = tskvParser.parse(key.toString() + '\t' + value.toString());
		} catch (RuntimeException e) {
			//System.err.println("bad parse" + e.toString());
			parsedRec = null;
		}
		
		//System.err.println(key.toString() + '\t' + value.toString());
		
		currentValue = TupleFactory.getInstance().newTuple(1);
		
		Map<String, String> mapToStore = new HashMap<String, String>();
		if (parsedRec != null) {
			mapToStore.putAll(parsedRec);
		}
		
		currentValue.set(0, mapToStore);	
		
	}
	
}
*/

class TSKVReadRecordStream extends MyKeyValueRecordReader {
	
	Map<String, String> mapToStore = new HashMap<String, String>();
	
	TSKVReadRecordStream(JobConf conf, InputStream stream, String filterPattern) throws IOException {
		super(conf, stream, filterPattern);
	}
	
	private boolean parseKeyValue(int from, int to, byte [] bytes) {
		int keyEnd = MyKeyValueLineRecordReader.findSeparator(bytes, 
			from, to - from, (byte)'=');
		
		if (keyEnd == -1) {
			return false;
		}
		
		String key = new String(TSKVUtils.dequote(bytes, from, keyEnd - from));
		String value = new String(TSKVUtils.dequote(bytes, keyEnd + 1, to - keyEnd - 1));
		
		mapToStore.put(key, value);
		
		return true;
	}
	
	protected void convertTextValueToTupleValue(Text key, Text value) throws IOException {
		mapToStore.clear();
		
		int offset = 0;
		while (offset != -1) {
			int nextOffset = MyKeyValueLineRecordReader.findSeparator(value.getBytes(), 
				offset, value.getLength() - offset, (byte)'\t');
			
			int offsetToStore = nextOffset != -1 ? nextOffset : value.getLength();
			if (offset != offsetToStore) {
				parseKeyValue(offset, offsetToStore, value.getBytes());
			}
			
			offset = nextOffset == -1 ? -1 : nextOffset + 1;	
		}
		
		currentValue = TupleFactory.getInstance().newTuple(1);
		currentValue.set(0, mapToStore);	
	}
	
}


class MyInputStreamFactory {
	public static org.apache.hadoop.mapreduce.RecordReader<Text, Tuple> construct(
		JobConf conf, String classname, InputStream fdStream,
		String filterPattern) throws IOException 
	{
		if (classname.equals("TSKVStorage")) {
			return new TSKVReadRecordStream(conf, fdStream, filterPattern);
		} else if (classname.equals("TSVStorage")) {
			return new TSVReadRecordStream(conf, fdStream, filterPattern);
		}
		
		return new MyKeyValueRecordReader(conf, fdStream, filterPattern);
	}
}

class MyMapExecutor implements Runnable {
	final JobConf conf;
	
	org.apache.hadoop.mapreduce.RecordReader<Text, Tuple> 
		inputStream;
	
	org.apache.hadoop.mapreduce.Mapper<Text, Tuple, PigNullableWritable, Writable>
		mapper;
	
	org.apache.hadoop.mapreduce.Mapper<Text, Tuple, PigNullableWritable, Writable>.Context
		mapContext;
	
	MyMapExecutor(JobConf cf, int inputIndex, 
			org.apache.hadoop.mapreduce.InputSplit split,
			FileSpec fileSpec,
			org.apache.hadoop.mapreduce.RecordWriter outputStream) throws IOException, InterruptedException 
	{
		this.conf = cf;
		
		InputStream fdStream 
			= new BufferedInputStream(new 
					FileInputStream("/proc/self/fd/" + (inputIndex * 2 + 3)));
		
		org.apache.hadoop.mapreduce.RecordReader<Text, Tuple>
			inputStream = MyInputStreamFactory.construct(cf, 
			fileSpec.getFuncName(), fdStream, 
			fileSpec.getFuncSpec().getCtorArgs() != null
			? fileSpec.getFuncSpec().getCtorArgs()[1]
			: null); 
	
		mapper = (org.apache.hadoop.mapreduce.Mapper<Text, Tuple, PigNullableWritable, Writable>)
			ReflectionUtils.newInstance(conf.getClass("mapreduce.map.class",
				org.apache.hadoop.mapreduce.Mapper.class,
				org.apache.hadoop.mapreduce.Mapper.class), conf);
		
		mapContext = mapper.new Context(conf, new TaskAttemptID(), inputStream, outputStream, 
			null, new MyStatusReporter(), split);
	}
	
	public void run() {
		try {
			mapper.run(mapContext);
		} catch (IOException e) {
			e.printStackTrace();
			return;
		} catch (InterruptedException e) {
			e.printStackTrace();
			return;
		}
	}
}

interface GroupPartitioner<T> {
	public String getPartition(T e) throws IOException;
}

class MRGroupPrimaryPartitioner implements GroupPartitioner<PigNullableWritable> {
	DataOutputBuffer buffer;
	MessageDigest digestObj;

	MRGroupPrimaryPartitioner(JobConf conf) throws NoSuchAlgorithmException {
		 buffer = new DataOutputBuffer();
		 digestObj = MessageDigest.getInstance("SHA");
	}
	
	@Override
	public String getPartition(PigNullableWritable e) throws IOException {
		DataOutputBuffer localBuffer = new DataOutputBuffer(); 
		
		if ((e.getIndex() & PigNullableWritable.mqFlag) != 0) {
			localBuffer.writeBytes(String.format("%03d", e.getIndex() & PigNullableWritable.idxSpace));
		}
		
		localBuffer.writeBytes(String.valueOf(e.isNull() ? '1' : '0'));
		if (!e.isNull()) {
			/* writing represantation */
			WritableComparable obj = (WritableComparable)e.getValueAsPigType();
			
			buffer.reset();
			obj.write(buffer);
			
			digestObj.reset();
			digestObj.update(buffer.getData());
			localBuffer.write(Base64.encodeBase64(digestObj.digest()));
		}
		localBuffer.writeBytes(String.format("%03d", e.getIndex() & PigNullableWritable.idxSpace));
		return new String(localBuffer.getData(), 0, localBuffer.getLength());
	}
}

class MRGroupSecondaryPartitioner implements GroupPartitioner<PigNullableWritable> {
	DataOutputBuffer buffer;
	MessageDigest digestObj;
	
	boolean isSecondarySort = false;
	byte keyType;
	
	MRGroupSecondaryPartitioner(JobConf conf) throws IOException, NoSuchAlgorithmException {
		keyType = Byte.valueOf(conf.get("pig.reduce.key.type"));
		isSecondarySort = MyUtil.isSecondarySort(conf);
		
		 buffer = new DataOutputBuffer();
		 digestObj = MessageDigest.getInstance("SHA");
	}
	
	@Override
	public String getPartition(PigNullableWritable e) throws IOException {	
		DataOutputBuffer localBuffer = new DataOutputBuffer(); 
		
		if ((e.getIndex() & PigNullableWritable.mqFlag) != 0) {
			localBuffer.writeBytes(String.format("%03d", e.getIndex() & PigNullableWritable.idxSpace));
		}

		localBuffer.writeBytes(String.valueOf(e.isNull() ? '1' : '0'));
		if (!e.isNull()) {
			/* writing represantation */
			PigNullableWritable obj = HDataType.getWritableComparableTypes(
					isSecondarySort ? ((Tuple)e.getValueAsPigType()).get(0) 
							: e.getValueAsPigType(), keyType);
			
			buffer.reset();
			obj.write(buffer);
			
			digestObj.reset();
			digestObj.update(buffer.getData());
			localBuffer.write(Base64.encodeBase64(digestObj.digest()));
		}
		localBuffer.writeBytes(String.format("%03d", e.getIndex() & PigNullableWritable.idxSpace));
		return new String(localBuffer.getData(), 0, localBuffer.getLength());
	}
}

class MRRecordWriterComplex 
extends org.apache.hadoop.mapreduce.RecordWriter<PigNullableWritable, Writable>
{	
	MyBase64Serializer<PigNullableWritable> keySerializer;
	MyBase64Serializer<Writable> valueSerializer;
	
	org.apache.hadoop.mapreduce.Partitioner<PigNullableWritable, Writable> partitioner;
	GroupPartitioner<PigNullableWritable> groupPartitioner; 
	
	public final byte OUTPUT_SEPARATOR = '\t';
	public final int NUM_REDUCES = 40000;
	
	MRRecordWriterComplex(JobConf conf) throws IOException, NoSuchAlgorithmException
	{
		keySerializer = new MyBase64Serializer<PigNullableWritable>(conf, 
                PigNullableWritable.class);
		valueSerializer = new MyBase64Serializer<Writable>(conf, Writable.class);

		if (conf.get("mapreduce.partitioner.class") != null) {
			partitioner = (org.apache.hadoop.mapreduce.lib.partition.HashPartitioner<PigNullableWritable, Writable>)
				ReflectionUtils.newInstance(conf.getClass("mapreduce.partitioner.class", null), conf);
		} else {
			partitioner = new org.apache.hadoop.mapreduce.lib.partition.HashPartitioner<PigNullableWritable, Writable>();
		}

		if (conf.get("mapred.output.value.groupfn.class") != null) {
			groupPartitioner = new MRGroupSecondaryPartitioner(conf);
		} else {
			groupPartitioner = new MRGroupPrimaryPartitioner(conf);
		} 
	}
	
	@Override
	public void close(TaskAttemptContext arg0) throws IOException,
			InterruptedException 
	{
		System.out.flush();
	}
	
	synchronized public void write(PigNullableWritable arg0, Writable arg1) throws IOException,
		InterruptedException 
	{
		ByteArrayOutputStream outStream = new ByteArrayOutputStream();
		
		int partition = partitioner.getPartition(arg0, arg1, NUM_REDUCES);
		String  secondPartition = groupPartitioner.getPartition(arg0);
		
		outStream.write(String.valueOf(partition).getBytes());
		outStream.write(OUTPUT_SEPARATOR);
		outStream.write(secondPartition.getBytes());
		outStream.write(OUTPUT_SEPARATOR);
		
		if (!arg0.isNull()) {
			byte[] encodedKey = keySerializer.serialize(arg0);
			outStream.write(MyUtil.stringizeByte(
					HDataType.findTypeFromNullableWritable(arg0)), 0, 2);
			outStream.write(encodedKey, 0, encodedKey.length);
		}
		
		outStream.write(OUTPUT_SEPARATOR);
		byte[] encodedValue = valueSerializer.serialize(arg1);
		outStream.write(encodedValue, 0, encodedValue.length);
		outStream.write('\n');
		
		System.out.write(outStream.toByteArray());
		
		
		/*
		System.out.print(String.valueOf(partition));
		System.out.write(OUTPUT_SEPARATOR);
		
		
		System.out.print(secondPartition);
		System.out.write(OUTPUT_SEPARATOR);
				
		if (!arg0.isNull()) {
			byte[] encodedKey = keySerializer.serialize(arg0);
			System.out.write(MyUtil.stringizeByte(
					HDataType.findTypeFromNullableWritable(arg0)), 0, 2);
			System.out.write(encodedKey, 0, encodedKey.length);
		}
		
		System.out.write(OUTPUT_SEPARATOR);
		
		byte[] encodedValue = valueSerializer.serialize(arg1);
		System.out.write(encodedValue, 0, encodedValue.length);
		System.out.println();
		*/
	}
};


interface ReduceRecordReader {

	public int getCurrentPartition();
	public String getCurrentSecondaryPartition();
	public Text getCurrentKeyRaw();
	public PigNullableWritable getCurrentKey();
	public NullableTuple getCurrentValue();
	public boolean getNext() throws IOException;

}

class MyReduceRecordReader implements ReduceRecordReader {
	
	MyKeyValueLineRecordReader reader = null;
	
	final byte DELIMITER = '\t'; 
	
	int currentPartition = 0;
	String currentSecondaryPartition = null;
	
	byte [] keyb = new byte[2];
	Text currentKey = new Text();
	
	Text currentPseudoKey = null;
	Text currentPseudoValue = null;
	
	PigNullableWritable currentKeyObject = null;
	NullableTuple currentValueObject = new NullableTuple();
	
	MyBase64Deserializer<PigNullableWritable> keyDeserializer = null;
	MyBase64Deserializer<NullableTuple> valueDeserializer = null;
	
	DataOutputBuffer keySerializeBuffer = new DataOutputBuffer();
	
	public MyReduceRecordReader(JobConf conf) throws IOException {
		reader = new MyKeyValueLineRecordReader(conf, System.in, 64 << 20);
		
		currentPseudoKey = reader.createKey();
		currentPseudoValue = reader.createValue();
		
		keyDeserializer = new MyBase64Deserializer<PigNullableWritable>(
				conf, PigNullableWritable.class);
		valueDeserializer = new MyBase64Deserializer<NullableTuple>(
				conf, NullableTuple.class);
	}

	@Override
	public int getCurrentPartition() {
		return currentPartition;
	}

	@Override
	public String getCurrentSecondaryPartition() {
		return currentSecondaryPartition;
	}

	@Override
	public Text getCurrentKeyRaw() {
		return currentKey;
	}

	@Override
	public PigNullableWritable getCurrentKey() {
		return currentKeyObject;
	}

	@Override
	public NullableTuple getCurrentValue() {
		return currentValueObject;
	}

	@Override
	public boolean getNext() throws IOException {
		boolean result = reader.next(currentPseudoKey, currentPseudoValue);
		if (!result) {
			return false;
		}
		
		byte [] pseudoValue = currentPseudoValue.getBytes();
		int kStart, vStart;
		kStart = MyKeyValueLineRecordReader.findSeparator(pseudoValue, 
				0, pseudoValue.length, DELIMITER);
		
		if (kStart == -1) {
			return false;
		}
		
		vStart = MyKeyValueLineRecordReader.findSeparator(pseudoValue, 
				kStart + 1, pseudoValue.length - kStart - 1, DELIMITER);
		
		if (vStart == -1) {
			return false;
		}
		
		try {
			currentPartition = Integer.parseInt(currentPseudoKey.toString());
		} catch (NumberFormatException e) {
			System.err.println("partition is nonumber");
			return false;
		}
		
		currentSecondaryPartition = new String(pseudoValue, 0, kStart);
		
		try {
			/* determine key type */ 
			if (kStart + 1 == vStart) { // if empty key
				currentKeyObject = new NullableTuple();
				currentKeyObject.setNull(true);
			} else {
				System.arraycopy(pseudoValue, kStart + 1, keyb, 0, 2);
				currentKeyObject = HDataType.getWritableComparableTypes(
					MyUtil.destringizeByte(keyb));
				currentKeyObject = this.keyDeserializer.deserialize(pseudoValue, 
						2 + kStart + 1, vStart - kStart - 1 - 2, currentKeyObject);
				currentKeyObject = MyUtil.cloneWritable(currentKeyObject, 
					MyUtil.destringizeByte(keyb));
			}
			
			/* serializing raw key here */
			
			keySerializeBuffer.reset();
			currentKeyObject.write(keySerializeBuffer);
			
			currentKey.clear();
			currentKey.set(keySerializeBuffer.getData(), 0, keySerializeBuffer.getLength());
			
			currentValueObject = valueDeserializer.deserialize(pseudoValue,
					vStart + 1, pseudoValue.length - vStart - 1, currentValueObject);
			
		} catch (IOException e) {
			e.printStackTrace();
			
			return false;
		}
		
		return true;
	}
	
}


abstract class MineReduceRecordIterator {
	protected ReduceRecordReader reader;
	protected JobConf conf;
	
	MineReduceRecordIterator(JobConf conf) throws IOException {
		reader = new MyReduceRecordReader(conf);
		this.conf = conf;
	}
	
	public abstract PigNullableWritable getKey();
	public abstract Iterator<NullableTuple> getValueIterator();
	public abstract boolean getNext() throws IOException;
}


class MineReducePrimaryRecordIterator extends MineReduceRecordIterator {
	PigNullableWritable prevKey = null;
	boolean noMore = false;
	
	public MineReducePrimaryRecordIterator(JobConf conf) throws IOException {
		super(conf);
		
		if (!reader.getNext()) {
			noMore = true;
		}
	}
	
	public PigNullableWritable getKey() {
		//System.err.println("getKey()");
		//System.err.println(prevKey.toString());
		//System.err.println(MyUtil.getStackTrace());
		return prevKey;
	}
	
	boolean uniqueKeyArrived() {
		return prevKey != null
			&& !prevKey.equals(reader.getCurrentKey());
			//&& !MyUtil.joinKeysAreEqual(prevKey, reader.getCurrentKey());
	}
	
	boolean getKeyValue() throws IOException {
		
		//System.err.println("getKeyValue()");
		//System.err.println(MyUtil.getStackTrace()); 
		
		if (noMore || uniqueKeyArrived()) {
			//System.err.println("1");
			return false;
		}
		if (!reader.getNext()) {
			noMore = true;
			//System.err.println("2");
			return false;
		}
		
		if (uniqueKeyArrived()) {
			//System.err.println("3");
			return false;
		}
		
		//System.err.println("4");
		
		return true;
	}
	
	class ValueIterator implements Iterator<NullableTuple> {
		
		boolean hasNextElement = true;
		
		public boolean hasNext() {
			//System.err.println("hasNext()");
			//System.err.println(MyUtil.getStackTrace());
			
			if (hasNextElement) {
				return true;
			}
			
			try {
				if (!getKeyValue()) {
					return false;
				}
			} catch (IOException e) {
				e.printStackTrace();
				return false;
			}
			
			hasNextElement = true;
			return true;
		}

		@Override
		public NullableTuple next() {
			//System.err.println("next()");
			//System.err.println(MyUtil.getStackTrace());
			
			NullableTuple result = MyUtil.cloneTuple(reader.getCurrentValue());
			
			//System.err.println(result.toString());
			
			hasNextElement = false;
			return result;
		}

		public void remove() {
		}	
	}
	
	@Override
	public Iterator<NullableTuple> getValueIterator() {
		Iterator<NullableTuple> iterator = new ValueIterator();
		return iterator;
	}
	
	public boolean getNext() throws IOException {
		//System.err.println("getNext()");
		//System.err.println(MyUtil.getStackTrace());
		
		if (noMore) {
			return false;
		}
		
		if (prevKey != null) {
			/* exhausting unread records */
			while (getKeyValue()) {}
		}
		
		if (noMore) {
			return false;
		}
		
		assert(uniqueKeyArrived());
		prevKey = reader.getCurrentKey();
		return true;
	}
}


class MineReduceSecondaryRecordIterator extends MineReduceRecordIterator {
	
	RawComparator<PigNullableWritable> keyComparatorInternal;
	MyKeyComparator keyComparator;
	WritableComparator groupComparator;
	
	SortedDataBag sortedBag = null;

	boolean noMore = false;
	
	Iterator<Tuple> currBagIterator = null;
	
	TupleFactory tupleFactory = TupleFactory.getInstance();
	
	boolean bagExhausted = true;
	PigNullableWritable currentKey = null;
	PigNullableWritable currBagKey = null;
	PigNullableWritable prevCurrBagKey = null;
	NullableTuple currBagValue = null;
	MyValueIterator currValueIterator = null;
	
	static private class MyKeyComparator implements Comparator<Tuple> {
		RawComparator<PigNullableWritable> keyComparatorInternal;
		
		MyKeyComparator(RawComparator<PigNullableWritable> c) {
			keyComparatorInternal = c;
		}

		@Override
		public int compare(Tuple o1, Tuple o2) {
			try {
				return keyComparatorInternal.compare((PigNullableWritable)o1.get(0), 
						(PigNullableWritable)o2.get(0));
			} catch (ExecException e) {
				e.printStackTrace();
				return 0;
			}
		}
		
	}
	
	public MineReduceSecondaryRecordIterator(JobConf conf) throws IOException {
		super(conf);
		
		keyComparatorInternal = conf.getOutputKeyComparator();
		keyComparator = new MyKeyComparator(keyComparatorInternal);
		groupComparator = (WritableComparator) conf.getOutputValueGroupingComparator();
		
		sortedBag = new SortedDataBag(keyComparator);
		
		if (!reader.getNext()) {
			noMore = true;
		}
	}
	
	public PigNullableWritable getKey() {
		//System.err.println("getKey() " + prevCurrBagKey.toString());
		return prevCurrBagKey;
	}
	
	public Iterator<NullableTuple> getValueIterator() {
		//System.err.println("getValueIterator");
		return currValueIterator;
	}
		
	private Tuple createBagItem() throws ExecException {
		Tuple t = tupleFactory.newTuple(2);
		t.set(0, reader.getCurrentKey());
		t.set(1, MyUtil.cloneTuple(reader.getCurrentValue()));
		return t;
	}
	
	boolean refillBag() throws IOException {
		
		//System.err.println("refillBag()");
		
		if (noMore) {
			return false;
		}
	
		if (currentKey == null) {
			if (!reader.getNext()) {
				noMore = true;
				return false;
			}
			
			currentKey = reader.getCurrentKey();
		}	
		
		sortedBag.add(createBagItem());
		
		while (true) {
			if (!reader.getNext()) {
				noMore = true;
				break;
			}
			
			boolean inThisGroup 
				= groupComparator.compare(currentKey, reader.getCurrentKey()) == 0;
			
			//System.err.println("refillBag(): " + inThisGroup);
			
			if (!inThisGroup) {
				break;
			}
			
			sortedBag.add(createBagItem());
		}

		bagExhausted = false;
		currBagIterator = sortedBag.iterator();
		currBagKey = null;
		prevCurrBagKey = null;

		return true;
	}

	
	class MyValueIterator implements Iterator<NullableTuple> {
		
		boolean cachedResultExhausted = true;
		boolean lastCachedResult;
		
		boolean noMore = false;
		Iterator<Tuple> bagIterator = null;	

        boolean firstReadAvailable = false;
		
		MyValueIterator(Iterator<Tuple> bagIterator) {
			this.bagIterator = bagIterator;
            this.firstReadAvailable = currBagKey != null;
		}
		
		@Override
		public boolean hasNext() {
			//System.err.println("MyValueIterator::hasNext()");

            if (firstReadAvailable) {
                firstReadAvailable = false;
		        cachedResultExhausted = false;
		        lastCachedResult = true;
                return true;
            }
			
			if (noMore) {
				return false;
			}
			
			if (!cachedResultExhausted) {
				return lastCachedResult;
			}
			
			if (currBagKey == null) {
				if (!currBagIterator.hasNext()) {
					bagExhausted = true;
					sortedBag.clear();
					noMore = true;
					return false;
				}
				
				Tuple t = currBagIterator.next();
				try {
					currBagKey = (PigNullableWritable)t.get(0);
					currBagValue = (NullableTuple)t.get(1);
					prevCurrBagKey = currBagKey;
				} catch (ExecException e) {
					e.printStackTrace();
					return false;
				}
				
				cachedResultExhausted = false;
				lastCachedResult = true;
				
				return true;
			}
		
			if (!currBagIterator.hasNext()) {
				bagExhausted = true;
				sortedBag.clear();
				noMore = true;
				return false;
			}
			
			Tuple t = currBagIterator.next();
			PigNullableWritable k;
			try {
				k = (PigNullableWritable)t.get(0);
				currBagValue = (NullableTuple)t.get(1);
			} catch (ExecException e) {
				e.printStackTrace();
				return false;
			}
			
			boolean inThisGroup = keyComparatorInternal.compare(currBagKey, k) == 0;
			prevCurrBagKey = currBagKey;
			currBagKey = k;
			
			//System.err.println("inThisGroup: " + inThisGroup);
			
			if (!inThisGroup) {
				noMore = true;
				return false;
			}
			
			cachedResultExhausted = false;
			lastCachedResult = true;
			
			return true;
		}

		@Override
		public org.apache.pig.impl.io.NullableTuple next() {
			//System.err.println("MyValueIterator::next()");
			cachedResultExhausted = true;
			return currBagValue;
		}

		@Override
		public void remove() {
		}
		
	}
	
	
	
	
	
	public boolean getNext() throws IOException {
		//System.err.println("SecondaryIterator:getNext()");
		
		// exhaust it first
		if (currValueIterator != null) {
			while (currValueIterator.hasNext()) {
				currValueIterator.next();
			}
		}
		
		if (bagExhausted) {
			if (!refillBag()) {
				return false;
			}
		}
		
		//System.err.println("Initializing: currValueIterator");
		currValueIterator = new MyValueIterator(currBagIterator);
		
		// initialize currBagKey, etc
		currValueIterator.hasNext();
		
		return true;
	}
	

}



public class PigWrapper {
	
	/* args[0] - path to submitted job.xml */
	public static void main(String[] args) throws Exception {
		if (args.length != 3) {
			throw new RuntimeException("usage: <submit-dir> <mode> <map|reduce>");
		}
		
		Path submitDir = new Path(args[0]);
		final JobConf conf = new JobConf(new Path(submitDir, "job.xml"));
		conf.set("mypig.execution.mode", args[1]);
	
		{
		    String storesName = args[2].equals("map") 
	        		? JobControlCompiler.PIG_MAP_STORES
	        		: JobControlCompiler.PIG_REDUCE_STORES;
		    		    
		    List<POStore> stores 
		    	= (List<POStore>)ObjectSerializer.deserialize(
		    			conf.get(storesName));
		    		
		    for (int i = 0; i < stores.size(); ++i) {
		        FileSpec spec = stores.get(i).getSFile();
		 
		        String pigStorageFunction = "MyPigStorage('"
		        		+ spec.getFuncName()  + "','"
		                + spec.getFileName() + "','\t')";
		        
				if (!MyPigStorage.funcMap.containsKey(spec.getFileName())) {
					MyPigStorage.insertWriter(spec.getFileName(), 
						(RecordWriter)new MyRecordWriter(conf), 
						spec.getFuncName(), 
						spec.getFuncSpec().getCtorArgs() != null
						? spec.getFuncSpec().getCtorArgs()[1]
						: null);
				}
				
				FuncSpec fSpec = new FuncSpec(pigStorageFunction);
				stores.get(i).setSFile(new FileSpec(spec.getFileName(), fSpec)); 
		    }
		    
		    conf.set(storesName, ObjectSerializer.serialize((Serializable)stores));
		}
		
		String planName = args[2].equals("map") ? "pig.mapPlan" : "pig.reducePlan"; 
		PhysicalPlan mp = (PhysicalPlan) ObjectSerializer.deserialize(conf.get(planName));
		
		if (mp != null) {
			List<POStore> stores = PlanHelper.getPhysicalOperators(mp, POStore.class);
			
			for (int i = 0; i < stores.size(); ++i) {
				FileSpec spec = stores.get(i).getSFile();
				
		        String pigStorageFunction = "MyPigStorage('"
		        		+ spec.getFuncName()  + "','"
		                + spec.getFileName() + "','\t')";
				
				if (!MyPigStorage.funcMap.containsKey(spec.getFileName())) {
					MyPigStorage.insertWriter(spec.getFileName(), 
							(RecordWriter)new MyRecordWriter(conf), 
							spec.getFuncName(), spec.getFuncSpec().getCtorArgs()[1]);
				}
				
				FuncSpec fSpec = new FuncSpec(pigStorageFunction);
				stores.get(i).setSFile(new FileSpec(spec.getFileName(), fSpec)); 
			}
			conf.set(planName, ObjectSerializer.serialize(mp));
		}
		
		MyPigStorage.resolveFuncMap();

		if (args[2].equals("map")) {

			boolean isComplexWrite = MyUtil.isComplexWrite(conf);
			
			org.apache.hadoop.mapreduce.RecordWriter<PigNullableWritable, Writable>
				outputStream = null;
			if (isComplexWrite) {
				outputStream = new MRRecordWriterComplex(conf);
			} else {
				assert(MyPigStorage.firstTableWriter != null);
				outputStream = MyPigStorage.firstTableWriter;
			}
			
			if (conf.get("mapreduce.combine.class") != null) {
				outputStream = new MRCombiner(conf, outputStream);
			}
	
			ArrayList<FileSpec> inputs = (ArrayList<FileSpec>) ObjectSerializer
					.deserialize(conf.get("pig.inputs"));
			ArrayList<ArrayList<OperatorKey>>
				inpTargets = (ArrayList<ArrayList<OperatorKey>>) ObjectSerializer
						.deserialize(conf.get("pig.inpTargets"));
			
			ArrayList<PigSplit> inpSplits = new ArrayList<PigSplit>();
			
			for (int i = 0; i < inputs.size(); ++i) {
				FileSpec spec = inputs.get(i);
				inpSplits.add(new PigSplit(
					new org.apache.hadoop.mapreduce.InputSplit[] {new org.apache.hadoop.mapreduce.lib.input.FileSplit(
							new Path(spec.getFileName()), 0, 1, new String[] {"localhost"})},
					i, inpTargets.get(i), 0
				));
			}
			
			ArrayList<Thread> threads = new ArrayList<Thread>();
			for (int i = 0; i < inpSplits.size(); ++i) {
				Thread thread = new Thread(new MyMapExecutor(conf, i, 
					inpSplits.get(i), inputs.get(i), outputStream));
				thread.start();
				threads.add(thread);
				
			}
			
			/* joining threads */
			for (int i = 0; i < inpSplits.size(); ++i) {
				threads.get(i).join();
			}
			
			outputStream.close(null);
			
		} else if (args[2].equals("reduce")) {
			
			org.apache.hadoop.mapreduce.Reducer<PigNullableWritable, NullableTuple, PigNullableWritable, Writable> reducer 
			= (org.apache.hadoop.mapreduce.Reducer<PigNullableWritable, NullableTuple, PigNullableWritable, Writable>)
				ReflectionUtils.newInstance(conf.getClass("mapreduce.reduce.class",
							org.apache.hadoop.mapreduce.Reducer.class), conf);
			
			Counters cnts = new Counters();
			final Counter inInputKeyCounter = cnts.getGroup("main").getCounterForName("mystatus1");
			final Counter inInputValueCounter = cnts.getGroup("main").getCounterForName("mystatus2");
	
			/* always will be non-null */
			final RawComparator groupComparator = conf.getOutputValueGroupingComparator();
			
			assert(MyPigStorage.firstTableWriter != null);
			org.apache.hadoop.mapreduce.Reducer<PigNullableWritable, NullableTuple, PigNullableWritable, Writable>.Context reduceContext
			= reducer.new Context(
					conf, 
					new TaskAttemptID(),
					new StubRawKeyValueIterator(),
					inInputKeyCounter,
					inInputValueCounter,
					MyPigStorage.firstTableWriter,
					null, 
					new MyStatusReporter(),
					null, 
					PigNullableWritable.class, NullableTuple.class
			) {     
			      MineReduceRecordIterator iterator = null;
			      PigNullableWritable prevKey = null;
			      boolean noMore = false;
			      Iterator<NullableTuple> currValIter = null;
			      boolean keyFilled = false;
			      
			      {
			    	  iterator = MyUtil.isSecondarySort(conf) ? new MineReduceSecondaryRecordIterator(conf)
			    	  		: new MineReducePrimaryRecordIterator(conf);
			      }

			  public boolean nextKey() throws IOException,InterruptedException {
				  
				  //System.err.println("nextKey()");  
				  
				if (noMore) {
					 return false;
				}
				
				if (keyFilled) {
					keyFilled = false;
					return true;
				}
			    
			    if (prevKey == null) {
			    	boolean result = iterator.getNext();
			    	if (result) {
			    		prevKey = iterator.getKey();
			    		currValIter = iterator.getValueIterator();
			    	}
			    	return result;
			    }
			    
			    while (true) {
			    	boolean result = iterator.getNext();
			    	if (!result) {
			    		noMore = true;
			    		return false;
			    	}
			    	
			    	/* is time to exit loop */
			    	if (groupComparator.compare(prevKey, iterator.getKey()) != 0) {
			    		prevKey = iterator.getKey();
			    		currValIter = iterator.getValueIterator();
			    		return true;
			    	}
			    }
			  }

			  /**
			   * Advance to the next key/value pair.
			   */
			  @Override
			  public boolean nextKeyValue() throws IOException, InterruptedException {				  
				throw new RuntimeException("not implemented");
			  }

			  public PigNullableWritable getCurrentKey() {
				//System.err.println("key=" + prevKey.toString());
			    return prevKey;
			  }

			  @Override
			  public NullableTuple getCurrentValue() {
				throw new RuntimeException("not implemented");
			  }
			  
			  class ValueIterator<NullableTuple> implements Iterator<NullableTuple> {
				   
				  
				@Override
				public boolean hasNext() {
					//System.err.println("hasNext()");
					
					if (noMore) {
						return false;
					}
					
					/* we are exhausted need getNext() */
					if (keyFilled) {
						return false;
					}
					
					if (currValIter == null || prevKey == null) {
						throw new RuntimeException("can't be here");
					}
					
					if (currValIter.hasNext()) {
						return true;
					}
					
					boolean result = false;
					try {
						result = iterator.getNext();
					} catch (IOException e) {
						e.printStackTrace();
						return false;
					}
					
					
					if (!result) {
						noMore = true;
						return false;
					}
					
					//System.err.println(prevKey.toString() + " " + iterator.getKey().toString());
					result = groupComparator.compare(prevKey, iterator.getKey()) == 0;
					
					prevKey = iterator.getKey();
					currValIter = iterator.getValueIterator();
					
					/* if unique key arrived */
					if (!result) {
						keyFilled = true;
					}
					
					//System.err.println("hasNext(): " + result);
					
					return result;
				}

				@Override
				public NullableTuple next() {
					//System.err.println("next()");
					
					if (currValIter.hasNext()) {
						return (NullableTuple) currValIter.next(); 
					}
					
					throw new RuntimeException("can't be here"); 
				}

				@Override
				public void remove() {
				}
				  
			  }
			  
			  class ValueIterable implements Iterable<NullableTuple> {
				  Iterator<NullableTuple> iterator;
				  
				  ValueIterable(ValueIterator<NullableTuple> it) {
					  iterator = it;
				  }
				  
				  public Iterator<NullableTuple> iterator() {
					  return iterator;
				  }
			  }

			  public 
			  Iterable<NullableTuple> getValues() throws IOException, InterruptedException {
				//System.err.println("getValues()"); 
			    return new ValueIterable(new ValueIterator());
			  }
			};
			
			reducer.run(reduceContext);
		
		} else {
			throw new RuntimeException("args[2] must be map, reduce or combiner");
		}
		
		MyPigStorage.closeWriters();
	}
}

class TupleSerializer {
	
	public static void main(String[] args) throws Exception {
		if (args.length != 1) {
			System.err.println("usage: <num-fields>");
			return;
		}
		
		int numFields = Integer.parseInt(args[0]);
		MyKeyValueLineRecordReader stream = new MyKeyValueLineRecordReader(new JobConf(), System.in, 64 << 20);
		
		Text key = stream.createKey();
		Text value = stream.createValue();
		Tuple valueTuple = TupleFactory.getInstance().newTuple(numFields);
		
		MyBase64Serializer<Tuple> serializer = new MyBase64Serializer<Tuple>(new JobConf(), Tuple.class) ;
		
		final String OUTPUT_SEPARATOR = "\t";
		
		while (stream.next(key, value)) {
			String[] tokens = value.toString().split(OUTPUT_SEPARATOR);
			
			for (int i = 0; i < tokens.length; ++i) {
				valueTuple.set(i, tokens[i]);
			}
			
			byte[] encoded = serializer.serialize(valueTuple);
			System.out.write(key.getBytes(), 0, key.getLength());
			System.out.write(OUTPUT_SEPARATOR.getBytes()[0]);
			System.out.write(encoded);
			System.out.println();
		}
	}
}

class TupleDeserializer {
	
	public static void main(String[] args) throws Exception {
		MyKeyValueLineRecordReader stream = new MyKeyValueLineRecordReader(new JobConf(), System.in, 64 << 20);
		
		Text key = stream.createKey();
		Text value = stream.createValue();
		Tuple valueTuple = TupleFactory.getInstance().newTuple();
		
		MyBase64Deserializer<Tuple> deserializer = new MyBase64Deserializer<Tuple>(new JobConf(), Tuple.class);
		
		final String OUTPUT_SEPARATOR = "\t";

		
		while (stream.next(key, value)) {	
			deserializer.deserialize(value.getBytes(), value.getLength(), valueTuple);
			
			System.out.print(key);
			System.out.print(OUTPUT_SEPARATOR);
			System.out.print(valueTuple.toString());
			
			/*
			for (int i = 0; i < valueTuple.size(); ++i) {
				System.out.print(valueTuple.get(i));
				System.out.print(OUTPUT_SEPARATOR);
			}
			*/
			
			System.out.println();
		}
	}
}

class ReduceDeserializer {
	
	public static void main(String[] args) throws Exception {
		
		final JobConf conf = new JobConf();
		
		MyReduceRecordReader stream = new MyReduceRecordReader(conf);
		
		while (stream.getNext()) {
			int partition = stream.getCurrentPartition();
			PigNullableWritable key = stream.getCurrentKey();
			NullableTuple value = stream.getCurrentValue();
			
			System.out.print(partition);
			System.out.print("\t");
			System.out.print(key.toString());
			System.out.print("\t");
			System.out.print(key.getValueAsPigType().toString());
			System.out.print("\t");
			System.out.print(value.getValueAsPigType().toString());
			System.out.println();
		}
	}
}


class StoreViewer {
	public static void main(String [] args) throws Exception {
		if (args.length != 2) {
			throw new RuntimeException("usage: <submit-dir> map|reduce");
		}
		
		final JobConf conf = new JobConf(new Path(args[0], "job.xml"));
		
		List<POStore> mapStores 
			= (List<POStore>)ObjectSerializer.deserialize(
					conf.get(JobControlCompiler.PIG_MAP_STORES));
		
		List<POStore> reduceStores 
		= (List<POStore>)ObjectSerializer.deserialize(
				conf.get(JobControlCompiler.PIG_REDUCE_STORES));
		
		HashSet<String> mapSet = new HashSet<String>();
		for (int i = 0; i < mapStores.size(); ++i) {
			FileSpec spec = mapStores.get(i).getSFile();
			mapSet.add(spec.getFileName());
		}
		
		HashSet<String> reduceSet = new HashSet<String>();
		for (int i = 0; i < reduceStores.size(); ++i) {
			FileSpec spec = reduceStores.get(i).getSFile();
			reduceSet.add(spec.getFileName());
		}
		
		HashSet<String> result = new HashSet<String>();
		
		if (args[1].equals("map")) {
			result = mapSet;
		} else if (args[1].equals("reduce")) {
			result = reduceSet;	
		}
		
		String [] arr = result.toArray(new String[0]);
		Arrays.sort(arr);
		for (int i = 0; i < arr.length; ++i) {
			System.out.println(arr[i]);
		}
	}
}

class LoadViewer {
	public static void main(String [] args) throws Exception {
		if (args.length != 1) {
			throw new RuntimeException("usage: <submit-dir>");
		}
		
		final JobConf conf = new JobConf(new Path(args[0], "job.xml"));
		ArrayList<FileSpec> inputs = (ArrayList<FileSpec>) ObjectSerializer
				.deserialize(conf.get("pig.inputs"));
		
		ArrayList<String> st = new ArrayList<String>();
		for (int i = 0; i < inputs.size(); ++i) {
			FileSpec spec = inputs.get(i);
			System.out.println(spec.getFileName());
		}
	}
}

class RequestedReducerNumberViewer {
	public static void main(String [] args) throws Exception {
		if (args.length != 1) {
			throw new RuntimeException("usage: <submit-dir>");
		}
		
		final JobConf conf = new JobConf(new Path(args[0], "job.xml"));
		System.out.println(conf.getInt("pig.info.reducers.requested.parallel", -1));
	}
}







