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

#include "../contrib/nlohmann/json_fwd.hpp"

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
	///		Insert attribute.
	///	</summary>
	void InsertAttribute(
		const std::string & strKey,
		const std::string & strValue
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
	///		Set of attributes considered "key" attributes.
	///	</summary>
	std::set<std::string> m_setKeyAttributeNames;

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

	///	<summary>
	///		Convert to a JSON object.
	///	</summary>
	void ToJSON(
		nlohmann::json & j
	) const;

	///	<summary>
	///		Initialize from a JSON object.
	///	</summary>
	void FromJSON(
		const std::string & strKey,
		nlohmann::json & j
	);

public:
	///	<summary>
	///		NcType for the SubAxis.
	///	</summary>
	NcType m_nctype;

	///	<summary>
	///		Size of the SubAxis.
	///	</summary>
	long m_lSize;

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
		m_eType(Type_Unknown)
	{ }

	///	<summary>
	///		Constructor.
	///	</summary>
	AxisInfo(
		const std::string & strName
	) :
		DataObjectInfo(strName),
		m_eType(Type_Unknown)
	{ }

public:
	///	<summary>
	///		Dimension type.
	///	</summary>
	Type m_eType;

	///	<summary>
	///		A map from subaxis id to SubAxis.
	///	</summary>
	SubAxisVector m_vecSubAxis;
};

///	<summary>
///		A map from a dimension name to AxisInfo structure.
///	</summary>
typedef std::map<std::string, AxisInfo> AxisInfoMap;

///	<summary>
///		An Axis-SubAxis pair.
///	</summary>
typedef std::pair<std::string, std::string> AxisSubAxisPair;

///	<summary>
///		A subaxis coordinate.
///	</summary>
typedef std::map<std::string, std::string> AxisSubAxisMap;

///	<summary>
///		A subaxis coordinate.
///	</summary>
typedef std::set<AxisSubAxisPair> SubAxisCoordinate;

///	<summary>
///		A list of axis names.
///	</summary>
class AxisNameVector : public std::vector<std::string> {
public:
	///	<summary>
	///		Convert to a string.
	///	</summary>
	std::string ToString() const;

	///	<summary>
	///		Convert to JSON.
	///	</summary>
	void ToJSON(nlohmann::json & j) const;
};

///	<summary>
///		A list of subaxis names.
///	</summary>
typedef std::vector<std::string> SubAxisIdVector;

///	<summary>
///		A map from subaxis ids to file ids.
///	</summary>
class SubAxisToFileIdMap : public std::map< SubAxisIdVector, std::string> {
public:
	///	<summary>
	///		Convert to a string.
	///	</summary>
	std::string ToString() const;

	///	<summary>
	///		Convert to JSON.
	///	</summary>
	void ToJSON(nlohmann::json & j) const;
};

///	<summary>
///		A map from a vector of axis names to a SubAxisToFileIdMap.
///	</summary>
typedef std::map<AxisNameVector, SubAxisToFileIdMap> AxisNamesToSubAxisToFileIdMapMap;

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
		DataObjectInfo(strName)
	{ } 

	///	<summary>
	///		Insert a new SubAxisToFileIdMap from a JSON object.
	///	</summary>
	void SubAxisToFileIdMapFromJSON(
		const std::string & strKey,
		nlohmann::json & j
	);

public:
	///	<summary>
	///		Dimension names.
	///	</summary>
	std::vector<std::string> m_vecDimNames;

	///	<summary>
	///		Map from Times to filename index and time index.
	///	</summary>
	VariableTimeFileMap m_mapTimeFile;

	///	<summary>
	///		Map from subaxis coordinate to file id.
	///	</summary>
	AxisNamesToSubAxisToFileIdMapMap m_mapSubAxisToFileIdMaps;
};

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
	AxisSubAxisMap m_mapAxisSubAxis;

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
	)
	{ }

public:
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
/*
	///	<summary>
	///		Get the information on the specified dimension.
	///	</summary>
	const AxisInfo & GetAxisInfo(
		const std::string & strAxisName
	) const {
		AxisInfoMap::const_iterator iterAxisInfo =
			m_mapAxisInfo.find(strAxisName);
		if (iterAxisInfo == m_mapAxisInfo.end()) {
			_EXCEPTIONT("Invalid dimension");
		}
		return (iterAxisInfo->second);
	}
*/
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
		const std::string & strBaseDir,
		const std::vector<std::string> & strFilenames
	);

public:
	///	<summary>
	///		Output the time-variable index as a CSV.
	///	</summary>
	std::string OutputTimeVariableIndexCSV(
		const std::string & strCSVOutput
	);

	///	<summary>
	///		Output the indexed dataset as a XML file.
	///	</summary>
	std::string ToXMLFile(
		const std::string & strXMLOutputFilename
	) const;

	///	<summary>
	///		Read the indexed dataset from a JSON file.
	///	</summary>
	std::string FromJSONFile(
		const std::string & strJSONInputFilename
	);

	///	<summary>
	///		Output the indexed dataset as a JSON file.
	///	</summary>
	std::string ToJSONFile(
		const std::string & strJSONOutputFilename,
		bool fPrettyPrint = true
	) const;

protected:
	///	<summary>
	///		The DataObjectInfo describing this global dataset.
	///	</summary>
	DataObjectInfo m_datainfo;

	///	<summary>
	///		The base directory.
	///	</summary>
	std::string m_strBaseDir;

	///	<summary>
	///		Information on files that appear in the IndexedDataset.
	///	</summary>
	LookupVectorHeap<std::string, FileInfo> m_vecFileInfo;

	///	<summary>
	///		Information on variables that appear in the IndexedDataset.
	///	</summary>
	LookupVectorHeap<std::string, VariableInfo> m_vecVariableInfo;

	///	<summary>
	///		Information on axes that appear in the IndexedDataset.
	///	</summary>
	LookupVectorHeap<std::string, AxisInfo> m_vecAxisInfo;
};

///////////////////////////////////////////////////////////////////////////////

#endif

