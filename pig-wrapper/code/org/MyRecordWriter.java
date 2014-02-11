package org;

import java.io.IOException;
import java.io.OutputStream;
import java.util.Iterator;
import java.util.Map;
import java.util.Map.Entry;

import org.apache.hadoop.io.Writable;
import org.apache.hadoop.io.WritableComparable;
import org.apache.hadoop.mapred.JobConf;
import org.apache.hadoop.mapreduce.RecordWriter;
import org.apache.hadoop.mapreduce.TaskAttemptContext;
import org.apache.pig.data.Tuple;


abstract class MyRecordWriterImpl extends RecordWriter<WritableComparable, Writable> {
	boolean isLocalMode;
	
	MyRecordWriterImpl(JobConf conf) {
		isLocalMode = conf.get("mypig.execution.mode").equals("local");
	}
	
	boolean inLocalMode() {
		return isLocalMode;
	}
}

class MySerializeRecordWriterImpl extends MyRecordWriterImpl {
	
	MyBase64Serializer<WritableComparable> keySerializer;
	MyBase64Serializer<Tuple> valueSerializer;
	OutputStream outStream;

	public MySerializeRecordWriterImpl(String args,
			JobConf conf, OutputStream stream) throws IOException {
		super(conf);
		keySerializer = new MyBase64Serializer<WritableComparable>(
				conf, WritableComparable.class);
		valueSerializer = new MyBase64Serializer<Tuple>(
				conf, Tuple.class);
		outStream = stream;
	}
	
	
	@Override
	public void write(WritableComparable key, Writable value) throws IOException {
		if (key != null) {
			byte[] k = keySerializer.serialize(key);
			outStream.write(k);
		}
		
		if (!inLocalMode()) {
			outStream.write('\t');	
		}
		
		outStream.write('\t');
		
		if (value != null) {
			byte[] v = valueSerializer.serialize((Tuple)value);
			outStream.write(v);
		}
		
		outStream.write('\n');
		
	}

	@Override
	public void close(TaskAttemptContext arg0) throws IOException,
			InterruptedException 
	{
		outStream.flush();
		outStream.close();
	}
}

class MyTSVRecordWriterImpl extends MyRecordWriterImpl {
	
	OutputStream outputStream; 
	
	public MyTSVRecordWriterImpl(String args,
			JobConf conf, OutputStream stream) {
		super(conf);
		outputStream = stream;
	}

	@Override
	public void close(TaskAttemptContext arg0) throws IOException,
			InterruptedException 
	{
		outputStream.flush();
		outputStream.close();
	}

	@Override
	public void write(WritableComparable key, Writable value)
			throws IOException, InterruptedException 
	{
		
		/* always empty key for now */
		
		outputStream.write('\t');
		
		if (!inLocalMode()) {
			outputStream.write('\t');	
		}
	
		Tuple val = (Tuple)value;
		for (int i = 0; i < val.size(); ++i) {
			String result = val.get(i) == null ? "null" 
					: val.get(i).toString();
			for (int j = 0; j < result.length(); ++j) {
				if (result.charAt(j) == '\t') {
					outputStream.write('\\');
					outputStream.write('t');
					continue;
				}
				
				if (result.charAt(j) == '\n') {
					outputStream.write('\\');
					outputStream.write('n');
					continue;
				}
				
				outputStream.write(result.charAt(j));
			}
			
			if (i != val.size() - 1) {
				outputStream.write('\t');
			}
		}
		
		outputStream.write('\n');
	}

}


class MyTSKVRecordWriterImpl extends MyRecordWriterImpl {
	
	OutputStream outputStream; 
	String [] schema;
	
	public MyTSKVRecordWriterImpl(String args,
			JobConf conf, OutputStream stream) {
		super(conf);
		outputStream = stream;
		schema = args != null ? args.split("\\s*,\\s*") : null;
	}

	@Override
	public void close(TaskAttemptContext arg0) throws IOException,
			InterruptedException 
	{
		outputStream.flush();
		outputStream.close();
	}
	
	void appendQuotedKey(StringBuilder rowBuilder, String key) {
		
		for (int i = 0; i < key.length(); ++i) {
			switch (key.charAt(i)) {
			case '\t':
			rowBuilder.append('\\');
			rowBuilder.append('t');
			break;
			
			case '\n':
			rowBuilder.append('\\');
			rowBuilder.append('n');
			break;
			
			case '\f':
			rowBuilder.append('\\');
			rowBuilder.append('f');
			break;
			
			case '\r':
			rowBuilder.append('\\');
			rowBuilder.append('r');
			break;
			
			case '=':
			rowBuilder.append('\\');
			rowBuilder.append('=');
			break;		
			
			default:
			rowBuilder.append(key.charAt(i));
			}
		}
	}
	
	void appendQuotedValue(StringBuilder rowBuilder, String val) {
		
		for (int i = 0; i < val.length(); ++i) {
			switch (val.charAt(i)) {
			case '\t':
			rowBuilder.append('\\');
			rowBuilder.append('t');
			break;
			
			case '\n':
			rowBuilder.append('\\');
			rowBuilder.append('n');
			break;
			
			case '\f':
			rowBuilder.append('\\');
			rowBuilder.append('f');
			break;
			
			case '\r':
			rowBuilder.append('\\');
			rowBuilder.append('r');
			break;
			
			default:
			rowBuilder.append(val.charAt(i));
			}
		}
	}
	

	@Override
	public void write(WritableComparable key, Writable value)
			throws IOException, InterruptedException 
	{
		
		/* always empty key for now */
		

		StringBuilder rowBuilder = new StringBuilder();
		rowBuilder.append('\t');
		
		if (!inLocalMode()) {
			rowBuilder.append('\t');	
		}
		
		Tuple val = (Tuple)value;
		
		rowBuilder.append('\t');
		if (schema != null) {
			for (int i = 0; i < schema.length; ++i) {
				appendQuotedKey(rowBuilder, schema[i]);
				rowBuilder.append('=');
				appendQuotedValue(rowBuilder, val.get(i) != null 
						? val.get(i).toString() : "null");
				rowBuilder.append('\t');
			}
		} else {
			for (int i = 0; i < val.size(); ++i) {
				Map<String, ? extends Object> mp = (Map<String, ? extends Object>)val.get(i);
				
				Iterator<?> it = mp.entrySet().iterator();
				while (it.hasNext()) {
					Map.Entry<String, ? extends Object> entry = (Entry<String, ? extends Object>) it.next();
					if (entry.getKey().length() > 0) {
						appendQuotedKey(rowBuilder, entry.getKey());
						rowBuilder.append('=');
						appendQuotedValue(rowBuilder, entry.getValue() != null ? entry.getValue().toString() : "null");
					}
					
					rowBuilder.append('\t');
				}
			}
		}
		
		rowBuilder.append('\n');
		
		outputStream.write(rowBuilder.toString().getBytes());
	}

}


class MySerializeStringRecordWriterImpl extends MyRecordWriterImpl {
	
	OutputStream outStream;
	
	public MySerializeStringRecordWriterImpl(String args,
			JobConf conf, OutputStream stream) throws IOException {
		super(conf);
		outStream = stream;
	}
	
	
	@Override
	public void write(WritableComparable key, Writable value) throws IOException {
		/* empty key for now */
		
		outStream.write('\t');
		
		if (!inLocalMode()) {
			outStream.write('\t');	
		}
		
		if (value != null) {
			String tupleRepr = ((Tuple)value).toString();
			outStream.write(tupleRepr.getBytes());
		}
		
		outStream.write('\n');
		
	}

	@Override
	public void close(TaskAttemptContext arg0) throws IOException,
			InterruptedException 
	{
		outStream.flush();
		outStream.close();
	}
}

class MyRecordWriterFactory {
	
	static public MyRecordWriterImpl construct(
			String className, String args, 
			JobConf conf, OutputStream stream) throws IOException 
	{
		if (className.equals("TSVStorage")) {
			
			return new MyTSVRecordWriterImpl(args, conf, stream);
			
		} else if (className.equals("TSKVStorage")) {
			return new MyTSKVRecordWriterImpl(args, conf, stream);
			
		} else if (className.equals("PigStrStorage")) {
			return new MySerializeStringRecordWriterImpl(args, conf, stream);
		}
		
		return new MySerializeRecordWriterImpl(args, conf, stream);
	}
}


//NullableTuple
public class MyRecordWriter extends RecordWriter<WritableComparable, Writable> implements MyComplexStream {
	
	JobConf conf = null;
	MyRecordWriterImpl outImpl = null;
	OutputStream outStream = null;
	
	public MyRecordWriter(JobConf conf) {
		this.conf = conf;
	}

	public void initOutputImpl(String className, String args) throws IOException {
		if (outImpl == null) {
			outImpl = MyRecordWriterFactory.construct(
				className, args, conf, outStream);
		}
	}
	
	public void setOutputStream(OutputStream s) {
		outStream = s;
	}

	@Override
	public void close(TaskAttemptContext arg0) throws IOException,
			InterruptedException {
		assert(outImpl != null);
		outImpl.close(arg0);		
	}

	@Override
	synchronized public void write(WritableComparable arg0, Writable arg1)
			throws IOException, InterruptedException {
		assert(outImpl != null);
		outImpl.write(arg0, arg1);
	}

	@Override
	public RecordWriter<WritableComparable, Writable> getParentStream() {
		return this;
	}
	
}