/// \file
/// \brief \b [Internal] Just a regular tree
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

#ifndef __DS_TREE_H
#define __DS_TREE_H

#include "Export.h"
#include "DS_List.h"
#include "DS_Queue.h"
#include "RakMemoryOverride.h"

/// The namespace DataStructures was only added to avoid compiler errors for commonly named data structures
/// As these data structures are stand-alone, you can use them outside of RakNet for your own projects if you wish.
namespace DataStructures
{
	template <class TreeType>
	class RAK_DLL_EXPORT Tree : public RakNet::RakMemoryOverride
	{
	public:
		Tree();
		Tree(TreeType &inputData);
		~Tree();
		void LevelOrderTraversal(DataStructures::List<Tree*> &output);
		void AddChild(TreeType &newData);
		void DeleteDecendants(void);

		TreeType data;
		DataStructures::List<Tree *> children;
	};

	template <class TreeType>
	Tree<TreeType>::Tree()
	{

	}

	template <class TreeType>
	Tree<TreeType>::Tree(TreeType &inputData)
	{
		data=inputData;
	}

	template <class TreeType>
	Tree<TreeType>::~Tree()
	{
	}

	template <class TreeType>
	void Tree<TreeType>::LevelOrderTraversal(DataStructures::List<Tree*> &output)
	{
		unsigned i;
		Tree<TreeType> *node;
		DataStructures::Queue<Tree<TreeType>*> queue;

		for (i=0; i < children.Size(); i++)
			queue.Push(children[i]);

		while (queue.Size())
		{
			node=queue.Pop();
			output.Insert(node);
			for (i=0; i < node->children.Size(); i++)
				queue.Push(node->children[i]);
		}
	}

	template <class TreeType>
	void Tree<TreeType>::AddChild(TreeType &newData)
	{
		children.Insert(new Tree(newData));
	}

	template <class TreeType>
	void Tree<TreeType>::DeleteDecendants(void)
	{
        DataStructures::List<Tree*> output;
		LevelOrderTraversal(output);
		unsigned i;
		for (i=0; i < output.Size(); i++)
			delete output[i];
	}
}

#endif
