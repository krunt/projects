package org;

import org.apache.hadoop.io.Writable;
import org.apache.hadoop.io.WritableComparable;
import org.apache.hadoop.mapreduce.RecordWriter;

public interface MyComplexStream {
	public RecordWriter<WritableComparable, Writable> getParentStream();
}
