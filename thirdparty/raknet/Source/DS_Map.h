/// \file
/// \brief \b [Internal] Map
///
/// This file is part of RakNet Copyright 2003 Kevin Jenkins.
///
/// Usage of RakNet is subject to the appropriate license agreement.
/// Creative Commons Licensees are subject to the
/// license found at
/// http://creativecommons.org/licenses/by-nc/2.5/
/// Single application licensees are subject to the license found at
/// http://www.jenkinssoftware.com/SingleApplicationLicense.html
/// Custom license users are subject to the terms therein.
/// GPL license users are subject to the GNU General Public
/// License as published by the Free
/// Software Foundation; either version 2 of the License, or (at your
/// option) any later version.

#ifndef __RAKNET_MAP_H
#define __RAKNET_MAP_H

#include "DS_OrderedList.h"
#include "Export.h"

// If I want to change this to a red-black tree, this is a good site: http://www.cs.auckland.ac.nz/software/AlgAnim/red_black.html
// This makes insertions and deletions faster.  But then traversals are slow, while they are currently fast.

/// The namespace DataStructures was only added to avoid compiler errors for commonly named data structures
/// As these data structures are stand-alone, you can use them outside of RakNet for your own projects if you wish.
namespace DataStructures
{
	/// The default comparison has to be first so it can be called as a default parameter.
	/// It then is followed by MapNode, followed by NodeComparisonFunc
	template <class key_type>
		int defaultMapKeyComparison(const key_type &a, const key_type &b)
	{
		if (a<b) return -1; if (a==b) return 0; return 1;
	}

	/// \note IMPORTANT! If you use defaultMapKeyComparison then call IMPLEMENT_DEFAULT_COMPARISON or you will get an unresolved external linker error.
	template <class key_type, class data_type, int (*key_comparison_func)(const key_type&, const key_type&)=defaultMapKeyComparison<key_type> >
	class RAK_DLL_EXPORT Map : public RakNet::RakMemoryOverride
	{
	public:
		static void IMPLEMENT_DEFAULT_COMPARISON(void) {DataStructures::defaultMapKeyComparison<key_type>(key_type(),key_type());}

		struct MapNode
		{
			MapNode() {}
			MapNode(key_type _key, data_type _data) : mapNodeKey(_key), mapNodeData(_data) {}
			MapNode& operator = ( const MapNode& input ) {mapNodeKey=input.mapNodeKey; mapNodeData=input.mapNodeData; return *this;}
			MapNode( const MapNode & input) {mapNodeKey=input.mapNodeKey; mapNodeData=input.mapNodeData;}
			key_type mapNodeKey;
			data_type mapNodeData;
		};

		// Has to be a static because the comparison callback for DataStructures::OrderedList is a C function
		static int NodeComparisonFunc(const key_type &a, const MapNode &b)
		{
#ifdef _MSC_VER
#pragma warning( disable : 4127 ) // warning C4127: conditional expression is constant
#endif
			return key_comparison_func(a, b.mapNodeKey);
		}

		Map();
		~Map();
		Map( const Map& original_copy );
		Map& operator= ( const Map& original_copy );

		data_type& Get(const key_type &key); 
		data_type Pop(const key_type &key);
		// Add if needed
		void Set(const key_type &key, const data_type &data);
		// Must already exist
		void SetExisting(const key_type &key, const data_type &data);
		// Must add
		void SetNew(const key_type &key, const data_type &data);
		bool Has(const key_type &key);
		bool Delete(const key_type &key);
		data_type& operator[] ( const unsigned int position ) const;
		key_type GetKeyAtIndex( const unsigned int position ) const;
		unsigned GetIndexAtKey( const key_type &key );
		void RemoveAtIndex(const unsigned index);
		void Clear(void);
		unsigned Size(void) const;

	protected:
		DataStructures::OrderedList< key_type,MapNode,Map::NodeComparisonFunc > mapNodeList;

		void SaveLastSearch(const key_type &key, unsigned index);
		bool HasSavedSearchResult(const key_type &key) const;

		unsigned lastSearchIndex;
		key_type lastSearchKey;
		bool lastSearchIndexValid;
	};

	template <class key_type, class data_type, int (*key_comparison_func)(const key_type&,const key_type&)>
	Map<key_type, data_type, key_comparison_func>::Map()
	{
		lastSearchIndexValid=false;
	}

	template <class key_type, class data_type, int (*key_comparison_func)(const key_type&,const key_type&)>
	Map<key_type, data_type, key_comparison_func>::~Map()
	{
		Clear();
	}

	template <class key_type, class data_type, int (*key_comparison_func)(const key_type&,const key_type&)>
	Map<key_type, data_type, key_comparison_func>::Map( const Map& original_copy )
	{
		mapNodeList=original_copy.mapNodeList;
        lastSearchIndex=original_copy.lastSearchIndex;
		lastSearchKey=original_copy.lastSearchKey;
		lastSearchIndexValid=original_copy.lastSearchIndexValid;
	}

    template <class key_type, class data_type, int (*key_comparison_func)(const key_type&,const key_type&)>
	Map<key_type, data_type, key_comparison_func>& Map<key_type, data_type, key_comparison_func>::operator= ( const Map& original_copy )
	{
		mapNodeList=original_copy.mapNodeList;
		lastSearchIndex=original_copy.lastSearchIndex;
		lastSearchKey=original_copy.lastSearchKey;
		lastSearchIndexValid=original_copy.lastSearchIndexValid;
		return *this;
	}

	template <class key_type, class data_type, int (*key_comparison_func)(const key_type&,const key_type&)>
	data_type& Map<key_type, data_type, key_comparison_func>::Get(const key_type &key)
	{
		if (HasSavedSearchResult(key))
			return mapNodeList[lastSearchIndex].mapNodeData;

		bool objectExists;
		unsigned index;
		index=mapNodeList.GetIndexFromKey(key, &objectExists);
		assert(objectExists);
		SaveLastSearch(key,index);
		return mapNodeList[index].mapNodeData;
	}

	template <class key_type, class data_type, int (*key_comparison_func)(const key_type&,const key_type&)>
	unsigned Map<key_type, data_type, key_comparison_func>::GetIndexAtKey( const key_type &key )
	{
		if (HasSavedSearchResult(key))
			return lastSearchIndex;

		bool objectExists;
		unsigned index;
		index=mapNodeList.GetIndexFromKey(key, &objectExists);
		if (objectExists==false)
		{
			assert(objectExists);
		}
		SaveLastSearch(key,index);
		return index;
	}

	template <class key_type, class data_type, int (*key_comparison_func)(const key_type&,const key_type&)>
	void Map<key_type, data_type, key_comparison_func>::RemoveAtIndex(const unsigned index)
	{
		mapNodeList.RemoveAtIndex(index);
		lastSearchIndexValid=false;
	}

	template <class key_type, class data_type, int (*key_comparison_func)(const key_type&,const key_type&)>
		data_type Map<key_type, data_type, key_comparison_func>::Pop(const key_type &key)
	{
		bool objectExists;
		unsigned index;
		if (HasSavedSearchResult(key))
			index=lastSearchIndex;
		else
		{
			index=mapNodeList.GetIndexFromKey(key, &objectExists);
			assert(objectExists);
		}		
		data_type tmp = mapNodeList[index].mapNodeData;
		mapNodeList.RemoveAtIndex(index);
		lastSearchIndexValid=false;
		return tmp;
	}

	template <class key_type, class data_type, int (*key_comparison_func)(const key_type&,const key_type&)>
	void Map<key_type, data_type, key_comparison_func>::Set(const key_type &key, const data_type &data)
	{
		bool objectExists;
		unsigned index;

		if (HasSavedSearchResult(key))
		{
			mapNodeList[lastSearchIndex].mapNodeData=data;
			return;
		}
		
		index=mapNodeList.GetIndexFromKey(key, &objectExists);

		if (objectExists)
		{
			SaveLastSearch(key,index);
			mapNodeList[index].mapNodeData=data;
		}
		else
		{
			SaveLastSearch(key,mapNodeList.Insert(key,MapNode(key,data), true));
		}
	}

	template <class key_type, class data_type, int (*key_comparison_func)(const key_type&,const key_type&)>
	void Map<key_type, data_type, key_comparison_func>::SetExisting(const key_type &key, const data_type &data)
	{
		bool objectExists;
		unsigned index;

		if (HasSavedSearchResult(key))
		{
			index=lastSearchIndex;
		}
		else
		{
			index=mapNodeList.GetIndexFromKey(key, &objectExists);
			assert(objectExists);
			SaveLastSearch(key,index);
		}		

		mapNodeList[index].mapNodeData=data;
	}	

	template <class key_type, class data_type, int (*key_comparison_func)(const key_type&,const key_type&)>
	void Map<key_type, data_type, key_comparison_func>::SetNew(const key_type &key, const data_type &data)
	{
#ifdef _DEBUG
		unsigned index;
		bool objectExists;
		index=mapNodeList.GetIndexFromKey(key, &objectExists);
		assert(objectExists==false);
#endif
		SaveLastSearch(key,mapNodeList.Insert(key,MapNode(key,data), true));
	}

	template <class key_type, class data_type, int (*key_comparison_func)(const key_type&,const key_type&)>
	bool Map<key_type, data_type, key_comparison_func>::Has(const key_type &key)
	{
		if (HasSavedSearchResult(key))
			return true;

		bool objectExists;
		unsigned index;
		index=mapNodeList.GetIndexFromKey(key, &objectExists);
		if (objectExists)
			SaveLastSearch(key,index);
		return objectExists;
	}

	template <class key_type, class data_type, int (*key_comparison_func)(const key_type&,const key_type&)>
	bool Map<key_type, data_type, key_comparison_func>::Delete(const key_type &key)
	{
		if (HasSavedSearchResult(key))
		{
			lastSearchIndexValid=false;
			mapNodeList.RemoveAtIndex(lastSearchIndex);   
			return true;
		}

		bool objectExists;
		unsigned index;
		index=mapNodeList.GetIndexFromKey(key, &objectExists);
		if (objectExists)
		{
			lastSearchIndexValid=false;
			mapNodeList.RemoveAtIndex(index);
			return true;
		}
		else
			return false;
	}

	template <class key_type, class data_type, int (*key_comparison_func)(const key_type&,const key_type&)>
	void Map<key_type, data_type, key_comparison_func>::Clear(void)
	{
		lastSearchIndexValid=false;
		mapNodeList.Clear();
	}

	template <class key_type, class data_type, int (*key_comparison_func)(const key_type&,const key_type&)>
	data_type& Map<key_type, data_type, key_comparison_func>::operator[]( const unsigned int position ) const
	{
		return mapNodeList[position].mapNodeData;
	}

	template <class key_type, class data_type, int (*key_comparison_func)(const key_type&,const key_type&)>
		key_type Map<key_type, data_type, key_comparison_func>::GetKeyAtIndex( const unsigned int position ) const
	{
		return mapNodeList[position].mapNodeKey;
	}

	template <class key_type, class data_type, int (*key_comparison_func)(const key_type&,const key_type&)>
	unsigned Map<key_type, data_type, key_comparison_func>::Size(void) const
	{
		return mapNodeList.Size();
	}

	template <class key_type, class data_type, int (*key_comparison_func)(const key_type&,const key_type&)>
	void Map<key_type, data_type, key_comparison_func>::SaveLastSearch(const key_type &key, const unsigned index)
	{
		lastSearchIndex=index;
		lastSearchKey=key;
		lastSearchIndexValid=true;
	}

	template <class key_type, class data_type, int (*key_comparison_func)(const key_type&,const key_type&)>
	bool Map<key_type, data_type, key_comparison_func>::HasSavedSearchResult(const key_type &key) const
	{
		return lastSearchIndexValid && key_comparison_func(key,lastSearchKey)==0;
	}
}

#endif
