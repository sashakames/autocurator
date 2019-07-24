///////////////////////////////////////////////////////////////////////////////
///
///	\file    IndexedDataset.h
///	\author  Paul Ullrich
///	\version July 23, 2019
///

#ifndef _INDEXEDDATASET_H_
#define _INDEXEDDATASET_H_

#include "Announce.h"
#include "TimeObj.h"
#include "DataArray1D.h"
#include "LookupVectorHeap.h"
#include "MathHelper.h"
#include "netcdfcpp.h"

#include <set>
#include <map>
#include <vector>
#include <string>

///////////////////////////////////////////////////////////////////////////////

class Variable;

///////////////////////////////////////////////////////////////////////////////

///	<summary>
///		A local (file,time) pair.
///	</summary>
typedef std::pair<size_t, int> LocalFileTimePair;

///	<summary>
///		A map from time indices to local (file,time) pairs.
///	</summary>
typedef std::map<size_t, LocalFileTimePair> VariableTimeFileMap;

///	<summary>
///		A map from attribute names to values.
///	</summary>
typedef std::map<std::string, std::string> AttributeMap;

///////////////////////////////////////////////////////////////////////////////

///	<summary>
///		A class for storing metadata about a data object from a IndexedDataset.
///	</summary>
class DataObjectInfo {

public:
	///	<summary>
	///		Default constructor.
	///	</summary>
	DataObjectInfo() :
		m_nctype(ncNoType)
	{ }

	///	<summary>
	///		Constructor.
	///	</summary>
	DataObjectInfo(
		const std::string & strName
	) :
		m_strName(strName),
		m_nctype(ncNoType)
	{ }

public:
	///	<summary>
	///		Populate from a NcFile.
	///	</summary>
	std::string FromNcFile(
		NcFile * ncfile
	);

	///	<summary>
	///		Populate from a NcVar.
	///	</summary>
	std::string FromNcVar(
		NcVar * var,
		bool fCheckConsistency
	);

	///	<summary>
	///		Remove redundancies in the list of other attributes.
	///	</summary>
	void RemoveRedundantOtherAttributes(
		const DataObjectInfo & doiMaster
	);

public:
	///	<summary>
	///		Equality operator.
	///	</summary>
	bool operator== (const DataObjectInfo & info) const {
		return (
			(m_strName == info.m_strName) &&
			(m_nctype == info.m_nctype) &&
			(m_strUnits == info.m_strUnits) &&
			(m_mapKeyAttributes == info.m_mapKeyAttributes) &&
			(m_mapOtherAttributes == info.m_mapOtherAttributes));
	}

	///	<summary>
	///		Inequality operator.
	///	</summary>
	bool operator!= (const DataObjectInfo & info) const {
		return !((*this) == info);
	}

public:
	///	<summary>
	///		Data object name.
	///	</summary>
	std::string m_strName;

	///	<summary>
	///		NcType for the Variable.
	///	</summary>
	NcType m_nctype;

	///	<summary>
	///		Units for the Variable.
	///	</summary>
	std::string m_strUnits;

	///	<summary>
	///		Key attributes for this Variable.
	///	</summary>
	AttributeMap m_mapKeyAttributes;

	///	<summary>
	///		Other attributes for this Variable.
	///	</summary>
	AttributeMap m_mapOtherAttributes;

};

///////////////////////////////////////////////////////////////////////////////

///	<summary>
///		A class that describes primitive variable information from a IndexedDataset.
///	</summary>
class VariableInfo : public DataObjectInfo {

public:
	///	<summary>
	///		Constructor.
	///	</summary>
	VariableInfo(
		const std::string & strName
	) :
		DataObjectInfo(strName),
		m_iTimeDimIx(-1),
		m_iVerticalDimIx(-1),
		m_nVerticalDimOrder(+1)
	{ } 

public:
	///	<summary>
	///		Index of time dimension or (-1) if time dimension doesn't exist.
	///	</summary>
	int m_iTimeDimIx;

	///	<summary>
	///		Index of the vertical dimension or (-1) if vertical dimension doesn't exist.
	///	</summary>
	int m_iVerticalDimIx;

	///	<summary>
	///		(+1) if the vertical coordinate is bottom-up, (-1) if top-down.
	///	</summary>
	int m_nVerticalDimOrder;

	///	<summary>
	///		Dimension names.
	///	</summary>
	std::vector<std::string> m_vecDimNames;

	///	<summary>
	///		Size of each dimension.
	///	</summary>
	std::vector<long> m_vecDimSizes;

	///	<summary>
	///		Auxiliary dimension names.
	///	</summary>
	std::vector<std::string> m_vecAuxDimNames;

	///	<summary>
	///		Size of each auxiliary dimension.
	///	</summary>
	std::vector<long> m_vecAuxDimSizes;

	///	<summary>
	///		Map from Times to filename index and time index.
	///	</summary>
	VariableTimeFileMap m_mapTimeFile;
};

///////////////////////////////////////////////////////////////////////////////

///	<summary>
///		A class that stores a range of the given dimension.
///	</summary>
class SubAxis : public DataObjectInfo {

public:
	///	<summary>
	///		Constructor.
	///	</summary>
	SubAxis() :
		m_nctype(ncNoType)
	{ }

	///	<summary>
	///		Verify that this range is monotonic.
	///	</summary>
	std::string VerifyMonotonic() const {
		return std::string("");
	}

	///	<summary>
	///		Equality operator.
	///	</summary>
	bool operator==(const SubAxis & dimrange) const;

	///	<summary>
	///		Convert to a Python list.
	///	</summary>
	std::string ValuesToString() const;

public:
	///	<summary>
	///		NcType for the SubAxis.
	///	</summary>
	NcType m_nctype;

	///	<summary>
	///		Dimension values as ints.
	///	</summary>
	std::vector<int> m_dValuesInts;

	///	<summary>
	///		Dimension values as floats.
	///	</summary>
	std::vector<float> m_dValuesFloat;

	///	<summary>
	///		Dimension values as doubles.
	///	</summary>
	std::vector<double> m_dValuesDouble;
};

///////////////////////////////////////////////////////////////////////////////

///	<summary>
///		A class that describes axes from a climate dataset.
///	</summary>
class AxisInfo : public DataObjectInfo {

public:
	///	<summary>
	///		Axis types.
	///	</summary>
	enum Type {
		Type_Unknown = (-1),
		Type_Auxiliary = 0,
		Type_Grid = 1,
		Type_Record = 2,
		Type_Vertical = 3
	};

public:
	///	<summary>
	///		Vector of subaxes.
	///	</summary>
	typedef LookupVectorHeap<std::string, SubAxis> SubAxisVector;

public:
	///	<summary>
	///		Constructor.
	///	</summary>
	AxisInfo() :
		DataObjectInfo(""),
		m_lSize(0),
		m_nOrder(0),
		m_eType(Type_Unknown)
	{ }

	///	<summary>
	///		Constructor.
	///	</summary>
	AxisInfo(
		const std::string & strName
	) :
		DataObjectInfo(strName),
		m_lSize(0),
		m_nOrder(0),
		m_eType(Type_Unknown)
	{ }

public:
	///	<summary>
	///		Equality operator.
	///	</summary>
	bool operator== (const AxisInfo & diminfo) const {
		return (
			((DataObjectInfo &)(*this) == (DataObjectInfo &)(diminfo)) &&
			(m_eType == diminfo.m_eType) &&
			(m_lSize == diminfo.m_lSize) &&
			(m_nOrder == diminfo.m_nOrder) &&
			(m_dValuesFloat == diminfo.m_dValuesFloat) &&
			(m_dValuesDouble == diminfo.m_dValuesDouble));
	}

	///	<summary>
	///		Inequality operator.
	///	</summary>
	bool operator!= (const AxisInfo & diminfo) const {
		return !((*this) == diminfo);
	}

	///	<summary>
	///		Convert to string.
	///	</summary>
	std::string ToString() const {
		std::string str;
		str += m_strName + " : ";
		str += std::to_string(m_eType) + " : ";
		str += std::to_string(m_lSize) + " : ";
		str += std::to_string(m_nOrder) + " : ";
		str += m_strUnits + "\n";
		str += "[";

		if (m_nctype == ncDouble) {
			for (int i = 0; i < m_dValuesDouble.size(); i++) {
				str += std::to_string(m_dValuesDouble[i]);
				if (i != m_dValuesDouble.size()-1) {
					str += ", ";
				}
			}

		} else if (m_nctype == ncFloat) {
			for (int i = 0; i < m_dValuesFloat.size(); i++) {
				str += std::to_string(m_dValuesFloat[i]);
				if (i != m_dValuesFloat.size()-1) {
					str += ", ";
				}
			}
		}

		str += "]";

		return str;
	}

public:
	///	<summary>
	///		Dimension type.
	///	</summary>
	Type m_eType;

	///	<summary>
	///		Dimension size.
	///	</summary>
	long m_lSize;

	///	<summary>
	///		Dimension order.
	///	</summary>
	int m_nOrder;

	///	<summary>
	///		A map from subaxis id to SubAxis.
	///	</summary>
	SubAxisVector m_vecSubAxis;

	///	<summary>
	///		Dimension values as floats.
	///	</summary>
	std::vector<float> m_dValuesFloat;

	///	<summary>
	///		Dimension values as doubles.
	///	</summary>
	std::vector<double> m_dValuesDouble;
};

///	<summary>
///		A map from a dimension name to AxisInfo structure.
///	</summary>
typedef std::map<std::string, AxisInfo> AxisInfoMap;

///	<summary>
///		An Axis-SubAxis pair.
///	</summary>
typedef std::pair<std::string, std::string> AxisSubAxisPair;

///////////////////////////////////////////////////////////////////////////////

///	<summary>
///		A class that describes dimension information from a IndexedDataset.
///	</summary>
class FileInfo : public DataObjectInfo {

public:
	///	<summary>
	///		Constructor.
	///	</summary>
	FileInfo(
		const std::string & strFilename
	) :
		m_strFilename(strFilename)
	{ }

public:
	///	<summary>
	///		File name of this File.
	///	</summary>
	std::string m_strFilename;

	///	<summary>
	///		A set of AxisSubAxisPairs stored in this file.
	///	</summary>
	std::vector<AxisSubAxisPair> m_vecAxisSubAxisPairs;

	///	<summary>
	///		A set of variables stored in this file?
	///	</summary>
	//std::vector<LocalVariableInfo> m_vecLocalVariableInfo;
};

///////////////////////////////////////////////////////////////////////////////

///	<summary>
///		A data structure describing a list of files.
///	</summary>
class IndexedDataset {

public:
	///	<summary>
	///		Invalid File index.
	///	</summary>
	static const size_t InvalidFileIx;

	///	<summary>
	///		Invalid Time index.
	///	</summary>
	static const size_t InvalidTimeIx;

	///	<summary>
	///		A value to denote that a dimension has inconsistent sizes
	///		across files.
	///	</summary>
	static const long InconsistentDimensionSizes;

public:
	///	<summary>
	///		Constructor.
	///	</summary>
	IndexedDataset(
		const std::string & strName
	) :
		m_strRecordDimName("time"),
		m_sReduceTargetIx(InvalidFileIx)
	{ }

	///	<summary>
	///		Destructor.
	///	</summary>
	~IndexedDataset();

public:
	///	<summary>
	///		Get the count of filenames.
	///	</summary>
	size_t GetFilenameCount() const {
		return m_vecFilenames.size();
	}

	///	<summary>
	///		Get the vector of filenames.
	///	</summary>
	const std::string & GetFilename(size_t f) const {
		if (f >= m_vecFilenames.size()) {
			_EXCEPTIONT("Index out of range");
		}
		return m_vecFilenames[f];
	}

	///	<summary>
	///		Get the VariableInfo associated with a given variable name.
	///	</summary>
	const VariableInfo * GetVariableInfo(
		const std::string & strVariableName
	) const {
		for (size_t i = 0; i < m_vecVariableInfo.size(); i++) {
			if (m_vecVariableInfo[i]->m_strName == strVariableName) {
				return (m_vecVariableInfo[i]);
			}
		}
		return NULL;
	}

	///	<summary>
	///		Populate from a search string.
	///	</summary>
	std::string PopulateFromSearchString(
		const std::string & strSearchString
	);

public:
	///	<summary>
	///		Get the record dimension name.
	///	</summary>
	const std::string & GetRecordDimName() const {
		return m_strRecordDimName;
	}

	///	<summary>
	///		Get the number of time indices in the file list.
	///	</summary>
	size_t GetTimeCount() const {
		return m_vecTimes.size();
	}

	///	<summary>
	///		Get the Time with the specified index.
	///	</summary>
	const Time & GetTime(int iTime) const {
		if (iTime >= m_vecTimes.size()) {
			_EXCEPTIONT("Out of range");
		}
		return m_vecTimes[iTime];
	}

	///	<summary>
	///		Get the vector of Times associated with the IndexedDataset.
	///	</summary>
	const std::vector<Time> & GetTimes() const {
		return m_vecTimes;
	}

	///	<summary>
	///		Get the information on the specified dimension.
	///	</summary>
	const AxisInfo & GetDimInfo(
		const std::string & strDimName
	) const {
		AxisInfoMap::const_iterator iterDimInfo =
			m_mapAxisInfo.find(strDimName);
		if (iterDimInfo == m_mapAxisInfo.end()) {
			_EXCEPTIONT("Invalid dimension");
		}
		return (iterDimInfo->second);
	}

	///	<summary>
	///		Get the size of the specified dimension.
	///	</summary>
	long GetDimSize(const std::string & strDimName) const {
		AxisInfoMap::const_iterator iter =
			m_mapAxisInfo.find(strDimName);

		if (iter != m_mapAxisInfo.end()) {
			return iter->second.m_lSize;
		}

		_EXCEPTIONT("Invalid dimension name");
	}

	///	<summary>
	///		Load the data from a particular variable into the given array.
	///	</summary>
	std::string LoadData_float(
		const std::string & strVariableName,
		const std::vector<long> & vecAuxIndices,
		DataArray1D<float> & data
	);

	///	<summary>
	///		Write the data from the given array to disk.
	///	</summary>
	std::string WriteData_float(
		const std::string & strVariableName,
		const std::vector<long> & vecAuxIndices,
		const DataArray1D<float> & data
	);

	///	<summary>
	///		Add a new variable from a template.
	///	</summary>
	std::string AddVariableFromTemplate(
		const IndexedDataset * pobjSourceIndexedDataset,
		const Variable * pvar,
		VariableInfo ** ppvarinfo
	);

	///	<summary>
	///		Add a new variable from a template and replace the vertical dimension.
	///	</summary>
	std::string AddVariableFromTemplateWithNewVerticalDim(
		const IndexedDataset * pobjSourceIndexedDataset,
		const Variable * pvar,
		const std::string & strVerticalDimName,
		VariableInfo ** ppvarinfo
	);

public:
	///	<summary>
	///		Get the size of the specified dimension.
	///	</summary>
	long GetDimensionSize(
		const std::string & strDimName
	) const;

protected:
	///	<summary>
	///		Sort the array of Times to keep m_vecTimes in
	///		chronological order.
	///	</summary>
	void SortTimeArray();

	///	<summary>
	///		Index variable data.
	///	</summary>
	std::string IndexVariableData(
		size_t sFileIxBegin = InvalidFileIx,
		size_t sFileIxEnd = InvalidFileIx
	);

public:
	///	<summary>
	///		Output the time-variable index as a CSV.
	///	</summary>
	std::string OutputTimeVariableIndexCSV(
		const std::string & strCSVOutput
	);

	///	<summary>
	///		Output the time-variable index as a XML.
	///	</summary>
	std::string OutputTimeVariableIndexXML(
		const std::string & strXMLOutput
	);

	///	<summary>
	///		Output the time-variable index as a JSON.
	///	</summary>
	std::string OutputTimeVariableIndexJSON(
		const std::string & strJSONOutput
	);

protected:
	///	<summary>
	///		The DataObjectInfo describing this global dataset.
	///	</summary>
	DataObjectInfo m_datainfo;

	///	<summary>
	///		The name of the record dimension (default "time")
	///	</summary>
	std::string m_strRecordDimName;

	///	<summary>
	///		The base directory.
	///	</summary>
	std::string m_strBaseDir;

	///	<summary>
	///		The list of filenames.
	///	</summary>
	std::vector<std::string> m_vecFilenames;

	///	<summary>
	///		Information on files that appear in the IndexedDataset.
	///	</summary>
	LookupVectorHeap<std::string, FileInfo> m_vecFileInfo;

	///	<summary>
	///		The format of the record variable.
	///	</summary>
	std::string m_strTimeUnits;

	///	<summary>
	///		The list of Times that appear in the IndexedDataset
	///		(in chronological order).
	///	</summary>
	std::vector<Time> m_vecTimes;

	///	<summary>
	///		A map from Time to m_vecTimes vector index
	///	</summary>
	std::map<Time, size_t> m_mapTimeToIndex;

	///	<summary>
	///		Information on variables that appear in the IndexedDataset.
	///	</summary>
	std::vector<VariableInfo *> m_vecVariableInfo;

	///	<summary>
	///		Information on variables that appear in the IndexedDataset.
	///	</summary>
	std::vector<AxisInfo *> m_vecAxisInfo;

	///	<summary>
	///		A set containing dimension information for this IndexedDataset.
	///	</summary>
	AxisInfoMap m_mapAxisInfo;

	///	<summary>
	///		Names of grid dimensions for this IndexedDataset.
	///	</summary>
	std::vector<std::string> m_vecGridDimNames;

	///	<summary>
	///		Filename index that is the target of reductions (output mode).
	///	</summary>
	size_t m_sReduceTargetIx;

	///	<summary>
	///		Filename index for each of the time indices (output mode).
	///	</summary>
	std::map<size_t, LocalFileTimePair> m_mapOutputTimeFile;
};

///////////////////////////////////////////////////////////////////////////////

#endif

