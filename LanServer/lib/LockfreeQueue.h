#ifndef __LOCKFREEQUEUE__H__
#define __LOCKFREEQUEUE__H__

template <class DATA>
class CLockfreeQueue
{
public:
	struct st_NODE
	{
		DATA	Data;
		st_NODE *pNext;
	};

	struct st_END_NODE
	{
		st_NODE *pEndNode;
		__int64 iUniqueNum;
	};
public:
	/////////////////////////////////////////////////////////////////////////
	// 생성자
	//
	// Parameters: 없음.
	// Return: 없음.
	/////////////////////////////////////////////////////////////////////////
	CLockfreeQueue()
	{
		_pMemoryPool = new CMemoryPool<st_NODE>(0);

		_lUseSize = 0;

		_pHead = (st_END_NODE *)_aligned_malloc(sizeof(st_END_NODE), 16);
		_pHead->iUniqueNum = 0;

		_pTail = (st_END_NODE *)_aligned_malloc(sizeof(st_END_NODE), 16);
		_pTail->iUniqueNum = 0;

		/////////////////////////////////////////////////////////////////////
		// 더미 노드 붙이기
		/////////////////////////////////////////////////////////////////////
		st_NODE *stDummyNode = _pMemoryPool->Alloc();
		stDummyNode->pNext = NULL;

		_pHead->pEndNode = stDummyNode;
		_pTail->pEndNode = stDummyNode;

		_iUniqueNumHead = 0;
		_iUniqueNumTail = 0;
	}

	/////////////////////////////////////////////////////////////////////////
	// 소멸자
	//
	// Parameters: 없음.
	// Return: 없음.
	/////////////////////////////////////////////////////////////////////////
	virtual ~CLockfreeQueue()
	{
		st_NODE *pTemp;

		while (_pHead != NULL)
		{
			pTemp = _pHead->pEndNode;
			_pHead->pEndNode = _pHead->pEndNode->pNext;
			_pMemoryPool->Free(pTemp);
		}

		delete _pMemoryPool;
		delete _pHead;
		delete _pTail;
	}

	/////////////////////////////////////////////////////////////////////////
	// 현재 사용중인 용량 얻기.
	//
	// Parameters: 없음.
	// Return: (int)사용중인 용량.
	/////////////////////////////////////////////////////////////////////////
	long	GetUseSize(void);

	/////////////////////////////////////////////////////////////////////////
	// 데이터가 비었는가 ?
	//
	// Parameters: 없음.
	// Return: (bool)true, false
	/////////////////////////////////////////////////////////////////////////
	bool	isEmpty(void);


	/////////////////////////////////////////////////////////////////////////
	// CPacket 포인터 데이타 넣음.
	//
	// Parameters: (DATA)데이타.
	// Return: (bool) true, false
	/////////////////////////////////////////////////////////////////////////
	bool	Put(DATA Data)
	{
		st_END_NODE pTail;
		st_NODE *pTailNext, *pNode = _pMemoryPool->Alloc();

		pNode->Data = Data;
		pNode->pNext = NULL;
		__int64 iUniqueNumTail = InterlockedIncrement64(&_iUniqueNumTail);

		while (1)
		{
			/////////////////////////////////////////////////////////////////
			// Tail 지역변수 저장
			/////////////////////////////////////////////////////////////////
			pTail.iUniqueNum = _pTail->iUniqueNum;
			pTail.pEndNode = _pTail->pEndNode;
			
			/////////////////////////////////////////////////////////////////
			// Tail->Next 저장
			/////////////////////////////////////////////////////////////////
			pTailNext = pTail.pEndNode->pNext;

			/////////////////////////////////////////////////////////////////
			// 일단 비교해보고 tail->next가 null이면
			// Unique값과 비교해서 바꿈
			/////////////////////////////////////////////////////////////////
			if (pTailNext == NULL)
			{
				if (InterlockedCompareExchangePointer((PVOID *)&pTail.pEndNode->pNext, pNode, pTailNext) == nullptr)
				{
					InterlockedCompareExchange128((LONG64 *)_pTail, iUniqueNumTail, (LONG64)pNode, (LONG64 *)&pTail);
					break;
				}
			}

			/////////////////////////////////////////////////////////////////
			// 일단 비교해보고 tail->next가 null아니면
			// 그냥 일단 Tail 밈
			/////////////////////////////////////////////////////////////////
			else
			{
				InterlockedCompareExchange128((LONG64 *)_pTail, iUniqueNumTail, (LONG64)pTailNext, (LONG64 *)&pTail);
			}
		}

		InterlockedIncrement64((LONG64 *)&_lUseSize);
		return true;
	}

	/////////////////////////////////////////////////////////////////////////
	// 데이타 빼서 가져옴.
	//
	// Parameters: (DATA *) 뽑은 데이터 넣어줄 포인터
	// Return: (bool) true, false
	/////////////////////////////////////////////////////////////////////////
	bool	Get(DATA *pOutData)
	{
		st_END_NODE pHead, pTail;
		st_NODE  *pHeadNext, *pNode;
		__int64 iUniqueNumHead = InterlockedIncrement64(&_iUniqueNumHead);

		if (_lUseSize == 0)
			return false;

		InterlockedDecrement64((LONG64 *)&_lUseSize);

		while (1)
		{
			/////////////////////////////////////////////////////////////////
			// Head 지역변수 저장
			/////////////////////////////////////////////////////////////////
			pHead.iUniqueNum = _pHead->iUniqueNum;
			pHead.pEndNode = _pHead->pEndNode;
			
			/////////////////////////////////////////////////////////////////
			// Tail 지역변수 저장
			/////////////////////////////////////////////////////////////////
			pTail.iUniqueNum = _pTail->iUniqueNum;
			pTail.pEndNode = _pTail->pEndNode;
			
			pHeadNext = pHead.pEndNode->pNext;

			/////////////////////////////////////////////////////////////////
			// Queue가 비었을 때
			/////////////////////////////////////////////////////////////////
			if (pHead.pEndNode == pTail.pEndNode)
			{
				if (pHeadNext == NULL)
					return false;
			}

			/////////////////////////////////////////////////////////////////
			// 일단 비교해보고 tail->next가 null아니면
			// 그냥 일단 Tail 밈
			/////////////////////////////////////////////////////////////////
			if (pTail.pEndNode->pNext != NULL)
			{
				__int64 iUniqueNumTail = InterlockedIncrement64(&_iUniqueNumTail);
				InterlockedCompareExchange128((LONG64 *)_pTail, iUniqueNumTail, (LONG64)pTail.pEndNode->pNext, (LONG64 *)&pTail);
			}

			else
			{
				if (pHeadNext != NULL)
				{
					*pOutData = pHeadNext->Data;
					if (InterlockedCompareExchange128((LONG64 *)_pHead, iUniqueNumHead, (LONG64)pHead.pEndNode->pNext, (LONG64 *)&pHead))
					{
						_pMemoryPool->Free(pHead.pEndNode);
						break;
					}
				}
			}
		}

		return true;
	}

	int GetBlockCount()	{ return _pMemoryPool->GetBlockCount(); }

private:
	CMemoryPool<st_NODE>	*_pMemoryPool;

	long					_lUseSize;

	st_END_NODE			*_pHead;
	st_END_NODE			*_pTail;

	__int64				_iUniqueNumHead;
	__int64				_iUniqueNumTail;
};

#endif