/*
 * Copyright (C) 2008 Search Solution Corporation. All rights reserved by Search Solution. 
 *
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met: 
 *
 * - Redistributions of source code must retain the above copyright notice, 
 *   this list of conditions and the following disclaimer. 
 *
 * - Redistributions in binary form must reproduce the above copyright notice, 
 *   this list of conditions and the following disclaimer in the documentation 
 *   and/or other materials provided with the distribution. 
 *
 * - Neither the name of the <ORGANIZATION> nor the names of its contributors 
 *   may be used to endorse or promote products derived from this software without 
 *   specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE. 
 *
 */

/**
 * Title:        CUBRID Java Client Interface<p>
 * Description:  CUBRID Java Client Interface<p>
 * @version 2.0
 */

package cubrid.jdbc.jci;

import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.sql.Date;
import java.sql.Time;
import java.sql.Timestamp;
import java.util.Calendar;

import javax.transaction.xa.Xid;

import cubrid.jdbc.driver.CUBRIDBlob;
import cubrid.jdbc.driver.CUBRIDClob;
import cubrid.jdbc.driver.CUBRIDLobHandle;
import cubrid.sql.CUBRIDOID;

class UOutputBuffer {
    private UConnection u_con;
    private DataOutputStream dataBuffer;
    private ByteArrayOutputStream byteBuffer;

    UOutputBuffer(UConnection ucon) throws IOException {
	this.u_con = ucon;
	initBuffer();
    }

    private void initBuffer() throws IOException {
	byteBuffer = new ByteArrayOutputStream(4096);
	dataBuffer = new DataOutputStream(byteBuffer);
    }

    void sendData() throws IOException {
	DataOutputStream os = new DataOutputStream(u_con.getOutputStream());
	byte b[] = byteBuffer.toByteArray();

	os.writeInt(b.length);
	os.write(u_con.getCASInfo());
	os.write(b);
	os.flush();
	byteBuffer = new ByteArrayOutputStream(4096);
	dataBuffer = new DataOutputStream(byteBuffer);
    }

    void newRequest(OutputStream out, byte func_code) throws IOException {
	initBuffer();
	dataBuffer.write(func_code);
    }

    void newRequest(byte func_code) throws IOException {
	initBuffer();
	dataBuffer.write(func_code);
    }

    int addInt(int intValue) throws IOException {
	dataBuffer.writeInt(4);
	dataBuffer.writeInt(intValue);
	return 8;
    }

    int addLong(long longValue) throws IOException {
	dataBuffer.writeInt(8);
	dataBuffer.writeLong(longValue);
	return 12;
    }

    int addByte(byte bValue) throws IOException {
	dataBuffer.writeInt(1);
	dataBuffer.writeByte(bValue);
	return 5;
    }

    int addBytes(byte[] value) throws IOException {
	return addBytes(value, 0, value.length);
    }

    int addBytes(byte[] value, int offset, int len) throws IOException {
	dataBuffer.writeInt(len);
	dataBuffer.write(value, offset, len);
	return len + 4;
    }

    int addNull() throws IOException {
	dataBuffer.writeInt(0);
	return 4;
    }

    int addStringWithNull(String str) throws IOException {
	byte[] b;

	try {
	    b = str.getBytes(u_con.getCharset());
	} catch (java.io.UnsupportedEncodingException e) {
	    b = str.getBytes();
	}

	dataBuffer.writeInt(b.length + 1);
	dataBuffer.write(b, 0, b.length);
	dataBuffer.writeByte((byte) 0);
	return b.length + 5;
    }

    int addDouble(double value) throws IOException {
	dataBuffer.writeInt(8);
	dataBuffer.writeDouble(value);
	return 12;
    }

    int addShort(short value) throws IOException {
	dataBuffer.writeInt(2);
	dataBuffer.writeShort(value);
	return 6;
    }

    int addFloat(float value) throws IOException {
	dataBuffer.writeInt(4);
	dataBuffer.writeFloat(value);
	return 8;
    }

    int addDate(Date value) throws IOException {
	dataBuffer.writeInt(14);
	writeDate(value);
	return 18;
    }

    private Calendar c = Calendar.getInstance();

    private void writeDate(Date date) throws IOException {
	c.setTime(date);
	dataBuffer.writeShort(c.get(Calendar.YEAR));
	dataBuffer.writeShort(c.get(Calendar.MONTH) + 1);
	dataBuffer.writeShort(c.get(Calendar.DAY_OF_MONTH));
	dataBuffer.writeShort((short) 0);
	dataBuffer.writeShort((short) 0);
	dataBuffer.writeShort((short) 0);
	dataBuffer.writeShort((short) 0);
    }

    int addTime(Time value) throws IOException {
	dataBuffer.writeInt(14);
	writeTime(value);
	return 18;
    }

    private void writeTime(Time date) throws IOException {
	c.setTime(date);
	dataBuffer.writeShort((short) 0);
	dataBuffer.writeShort((short) 0);
	dataBuffer.writeShort((short) 0);
	dataBuffer.writeShort(c.get(Calendar.HOUR_OF_DAY));
	dataBuffer.writeShort(c.get(Calendar.MINUTE));
	dataBuffer.writeShort(c.get(Calendar.SECOND));
	dataBuffer.writeShort((short) 0);
    }

    int addTimestamp(Timestamp value) throws IOException {
	dataBuffer.writeInt(14);
	writeTimestamp(value, false);
	return 18;
    }

    private void writeTimestamp(Timestamp date, boolean withMili) throws IOException {
	c.setTime(date);
	dataBuffer.writeShort(c.get(Calendar.YEAR));
	dataBuffer.writeShort(c.get(Calendar.MONTH) + 1);
	dataBuffer.writeShort(c.get(Calendar.DAY_OF_MONTH));
	dataBuffer.writeShort(c.get(Calendar.HOUR_OF_DAY));
	dataBuffer.writeShort(c.get(Calendar.MINUTE));
	dataBuffer.writeShort(c.get(Calendar.SECOND));
	if (withMili) {
	    dataBuffer.writeShort(c.get(Calendar.MILLISECOND));
	} else {
	    dataBuffer.writeShort((short) 0);
	}
    }

    int addDatetime(Timestamp value) throws IOException {
	dataBuffer.writeInt(14);
	writeTimestamp(value, true);
	return 18;
    }

    int addOID(CUBRIDOID value) throws IOException {
	byte[] b = value.getOID();

	if (b == null || b.length != UConnection.OID_BYTE_SIZE) {
	    b = new byte[UConnection.OID_BYTE_SIZE];
	}

	dataBuffer.writeInt(UConnection.OID_BYTE_SIZE);
	dataBuffer.write(b, 0, b.length);
	return UConnection.OID_BYTE_SIZE + 4;
    }

    int addXid(Xid xid) throws IOException {
	byte[] gid = xid.getGlobalTransactionId();
	byte[] bid = xid.getBranchQualifier();
	int msgSize = 12 + gid.length + bid.length;
	dataBuffer.writeInt(msgSize);
	dataBuffer.writeInt(xid.getFormatId());
	dataBuffer.writeInt(gid.length);
	dataBuffer.writeInt(bid.length);
	dataBuffer.write(gid, 0, gid.length);
	dataBuffer.write(bid, 0, bid.length);
	return msgSize + 4;
    }

    int addCacheTime(UStatementCacheData cache_data) throws IOException {
	int sec, usec;
	if (cache_data == null) {
	    sec = usec = 0;
	} else {
	    sec = (int) (cache_data.srvCacheTime >>> 32);
	    usec = (int) (cache_data.srvCacheTime);
	}
	dataBuffer.writeInt(8);
	dataBuffer.writeInt(sec);
	dataBuffer.writeInt(usec);
	return 12;
    }

    int addBlob(CUBRIDBlob value) throws IOException {
	return addLob(value.getLobHandle());
    }

    int addClob(CUBRIDClob value) throws IOException {
	return addLob(value.getLobHandle());
    }

    private int addLob(CUBRIDLobHandle lobHandle) throws IOException {
	byte[] packedLobHandle = lobHandle.getPackedLobHandle();

	dataBuffer.writeInt(packedLobHandle.length);
	dataBuffer.write(packedLobHandle, 0, packedLobHandle.length);
	return packedLobHandle.length + 4;
    }

    int writeParameter(byte type, Object value) throws UJciException, IOException {
	String stringData;

	if (value == null) {
	    return addNull();
	}

	switch (type) {
	case UUType.U_TYPE_NULL:
	    return addNull();
	case UUType.U_TYPE_CHAR:
	case UUType.U_TYPE_NCHAR:
	case UUType.U_TYPE_STRING:
	case UUType.U_TYPE_VARNCHAR:
	    stringData = UGetTypeConvertedValue.getString(value);
	    return addStringWithNull(stringData);
	case UUType.U_TYPE_NUMERIC:
	    stringData = UGetTypeConvertedValue.getString(value);
	    return addStringWithNull(stringData);
	case UUType.U_TYPE_BIT:
	case UUType.U_TYPE_VARBIT:
	    if ((value instanceof byte[]) && (((byte[]) value).length > 1)) {
		return addBytes(UGetTypeConvertedValue.getBytes(value));
	    } else {
		return addByte(UGetTypeConvertedValue.getByte(value));
	    }
	case UUType.U_TYPE_MONETARY:
	case UUType.U_TYPE_DOUBLE:
	    return addDouble(UGetTypeConvertedValue.getDouble(value));
	case UUType.U_TYPE_DATE:
	    return addDate(UGetTypeConvertedValue.getDate(value));
	case UUType.U_TYPE_TIME:
	    return addTime(UGetTypeConvertedValue.getTime(value));
	case UUType.U_TYPE_TIMESTAMP:
	    return addTimestamp(UGetTypeConvertedValue.getTimestamp(value));
	case UUType.U_TYPE_DATETIME:
	    return addDatetime(UGetTypeConvertedValue.getTimestamp(value));
	case UUType.U_TYPE_FLOAT:
	    return addFloat(UGetTypeConvertedValue.getFloat(value));
	case UUType.U_TYPE_SHORT:
	    return addShort(UGetTypeConvertedValue.getShort(value));
	case UUType.U_TYPE_INT:
	    return addInt(UGetTypeConvertedValue.getInt(value));
	case UUType.U_TYPE_BIGINT:
	    return addLong(UGetTypeConvertedValue.getLong(value));
	case UUType.U_TYPE_SET:
	case UUType.U_TYPE_MULTISET:
	case UUType.U_TYPE_SEQUENCE:
	    if (!(value instanceof CUBRIDArray))
		new UJciException(UErrorCode.ER_TYPE_CONVERSION);
	    return writeCollection((CUBRIDArray) value);
	case UUType.U_TYPE_OBJECT:
	    if (!(value instanceof CUBRIDOID)) {
		new UJciException(UErrorCode.ER_TYPE_CONVERSION);
	    }
	    return addOID((CUBRIDOID) value);
	case UUType.U_TYPE_BLOB:
	    if (!(value instanceof CUBRIDBlob)) {
		new UJciException(UErrorCode.ER_TYPE_CONVERSION);
	    }
	    return addBlob((CUBRIDBlob) value);
	case UUType.U_TYPE_CLOB:
	    if (!(value instanceof CUBRIDClob)) {
		new UJciException(UErrorCode.ER_TYPE_CONVERSION);
	    }
	    return addClob((CUBRIDClob) value);
	}

	return 0;
    }

    private int writeCollection(CUBRIDArray data) throws UJciException, IOException {
	int collection_size = 1;
	DataOutputStream saveStream = dataBuffer;
	ByteArrayOutputStream byteStream = new ByteArrayOutputStream(4096);
	dataBuffer = new DataOutputStream(byteStream);

	dataBuffer.writeByte((byte) data.getBaseType());
	Object[] values = (Object[]) data.getArray();
	if (values == null) {
	    return collection_size;
	}

	switch (data.getBaseType()) {
	case UUType.U_TYPE_BIT:
	case UUType.U_TYPE_VARBIT:
	    byte[][] byteValues = null;
	    if (values instanceof byte[][]) {
		byteValues = (byte[][]) values;
	    } else if (values instanceof Boolean[]) {
		byteValues = new byte[values.length][];
		for (int i = 0; i < byteValues.length; i++) {
		    if (((Boolean[]) values)[i] != null) {
			byteValues[i] = new byte[1];
			byteValues[i][0] = (((Boolean[]) values)[i].booleanValue() == true) ? (byte) 1 : (byte) 0;
		    } else {
			byteValues[i] = null;
		    }
		}
	    }

	    for (int i = 0; byteValues != null && i < byteValues.length; i++) {
		if (byteValues[i] == null) {
		    collection_size += addNull();
		} else {
		    collection_size += addBytes(byteValues[i]);
		}
	    }
	    break;
	case UUType.U_TYPE_NUMERIC:
	    for (int i = 0; i < values.length; i++) {
		if (values[i] == null) {
		    collection_size += addNull();
		} else {
		    collection_size += addStringWithNull(values[i].toString());
		}
	    }
	    break;
	case UUType.U_TYPE_SHORT:
	case UUType.U_TYPE_INT:
	case UUType.U_TYPE_BIGINT:
	case UUType.U_TYPE_FLOAT:
	case UUType.U_TYPE_DOUBLE:
	case UUType.U_TYPE_MONETARY:
	case UUType.U_TYPE_DATE:
	case UUType.U_TYPE_TIME:
	case UUType.U_TYPE_TIMESTAMP:
	case UUType.U_TYPE_DATETIME:
	case UUType.U_TYPE_OBJECT:
	case UUType.U_TYPE_BLOB:
	case UUType.U_TYPE_CLOB:
	case UUType.U_TYPE_CHAR:
	case UUType.U_TYPE_NCHAR:
	case UUType.U_TYPE_STRING:
	case UUType.U_TYPE_VARNCHAR:
	    for (int i = 0; i < values.length; i++) {
		if (values[i] == null) {
		    collection_size += addNull();
		} else {
		    collection_size += writeParameter((byte) data.getBaseType(), values[i]);
		}
	    }
	    break;
	case UUType.U_TYPE_NULL:
	default:
	    for (int i = 0; i < values.length; i++) {
		collection_size += addNull();
	    }
	}

	dataBuffer.close();
	dataBuffer = saveStream;

	dataBuffer.writeInt(collection_size);
	dataBuffer.write(byteStream.toByteArray());
	return collection_size + 4;
    }

}
