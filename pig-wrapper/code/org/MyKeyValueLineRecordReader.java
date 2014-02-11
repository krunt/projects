package org;

import java.io.IOException;
import java.io.InputStream;
import java.util.regex.Pattern;

import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.io.LongWritable;
import org.apache.hadoop.io.Text;
import org.apache.hadoop.mapred.FileSplit;
import org.apache.hadoop.mapred.LineRecordReader;
import org.apache.hadoop.mapred.RecordReader;

public class MyKeyValueLineRecordReader implements RecordReader<Text, Text> {
	  
	  private final LineRecordReader lineRecordReader;

	  private byte separator = (byte) '\t';

	  private LongWritable dummyKey;

	  private Text innerValue;

	  public Class getKeyClass() { return Text.class; }
	  
	  public Text createKey() {
	    return new Text();
	  }
	  
	  public Text createValue() {
	    return new Text();
	  }
	  
	  public MyKeyValueLineRecordReader(Configuration job, 
			  InputStream in, int maxLineLength)
	    throws IOException {
	    
	    lineRecordReader = new LineRecordReader(in, 
	    		0, Integer.MAX_VALUE, maxLineLength);
	    dummyKey = lineRecordReader.createKey();
	    innerValue = lineRecordReader.createValue();
	    String sepStr = job.get("key.value.separator.in.input.line", "\t");
	    this.separator = (byte) sepStr.charAt(0);
	  }

	  public static int findSeparator(byte[] utf, int start, int length, byte sep) {
	    for (int i = start; i < (start + length); i++) {
	      if (utf[i] == sep) {
	        return i;
	      }
	    }
	    return -1;
	  }

	  /** Read key/value pair in a line. */
	  public synchronized boolean next(Text key, Text value)
	    throws IOException {
	    Text tKey = key;
	    Text tValue = value;
	    byte[] line = null;
	    int lineLen = -1;
	    if (lineRecordReader.next(dummyKey, innerValue)) {
	      line = innerValue.getBytes();
	      lineLen = innerValue.getLength();
	    } else {
	      return false;
	    }
	    if (line == null)
	      return false;
	    int pos = findSeparator(line, 0, lineLen, this.separator);
	    if (pos == -1) {
	      tKey.set(line, 0, lineLen);
	      tValue.set("");
	    } else {
	      int keyLen = pos;
	      byte[] keyBytes = new byte[keyLen];
	      System.arraycopy(line, 0, keyBytes, 0, keyLen);
	      int valLen = lineLen - keyLen - 1;
	      byte[] valBytes = new byte[valLen];
	      System.arraycopy(line, pos + 1, valBytes, 0, valLen);
	      tKey.set(keyBytes);
	      tValue.set(valBytes);
	    }
	    return true;
	  }
	  
	  public float getProgress() throws IOException {
	    return lineRecordReader.getProgress();
	  }
	  
	  public synchronized long getPos() throws IOException {
	    return lineRecordReader.getPos();
	  }

	  public synchronized void close() throws IOException { 
	    lineRecordReader.close();
	  }
	}

class MyKeyValueLineRecordReaderFiltered extends MyKeyValueLineRecordReader {
	
	private Pattern regexFilter = null; 
	
	public MyKeyValueLineRecordReaderFiltered(Configuration job, 
			  InputStream in, int maxLineLength, String filterPattern) throws IOException 
	{
		super(job, in, maxLineLength);
		regexFilter = Pattern.compile(filterPattern);
	}
	
	public synchronized boolean next(Text key, Text value) throws IOException {
		while (super.next(key, value)) {
			
			if (regexFilter.matcher(value.toString()).find()) {
				return true;
			}
			
		}
		
		return false;
	}
	
}

















