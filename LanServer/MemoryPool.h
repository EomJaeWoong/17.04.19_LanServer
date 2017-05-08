/*---------------------------------------------------------------

	procademy MemoryPool.

	메모리 풀 클래스.
	특정 데이타(구조체,클래스,변수)를 일정량 할당 후 나눠쓴다.

	- 사용법.

	procademy::CMemoryPool<DATA> MemPool(300, FALSE);
	DATA *pData = MemPool.Alloc();

	pData 사용

	MemPool.Free(pData);


	!.	아주 자주 사용되어 속도에 영향을 줄 메모리라면 생성자에서
		Lock 플래그를 주어 페이징 파일로 복사를 막을 수 있다.
		아주 중요한 경우가 아닌이상 사용 금지.

		
		
		주의사항 :	단순히 메모리 사이즈로 계산하여 메모리를 할당후 메모리 블록을 리턴하여 준다.
					클래스를 사용하는 경우 클래스의 생성자 호출 및 클래스정보 할당을 받지 못한다.
					클래스의 가상함수, 상속관계가 전혀 이뤄지지 않는다.
					VirtualAlloc 으로 메모리 할당 후 memset 으로 초기화를 하므로 클래스정보는 전혀 없다.
		
				
----------------------------------------------------------------*/
#ifndef  __MEMORYPOOL__H__
#define  __MEMORYPOOL__H__s
#include <assert.h>
#include <new.h>


	template <class DATA>
	class CMemoryPool
	{
	private:

		/* **************************************************************** */
		// 각 블럭 앞에 사용될 노드 구조체.
		/* **************************************************************** */
		struct st_BLOCK_NODE
		{
			st_BLOCK_NODE()
			{
				stpNextBlock = NULL;
			}
			st_BLOCK_NODE *stpNextBlock;
		};

	public:

		//////////////////////////////////////////////////////////////////////////
		// 생성자, 파괴자.
		//
		// Parameters:	(int) 최대 블럭 개수.
		//				(bool) 메모리 Lock 플래그 - 중요하게 속도를 필요로 한다면 Lock.
		// Return:
		//////////////////////////////////////////////////////////////////////////
		CMemoryPool(int iBlockNum, bool bLockFlag = false)
		{
			st_BLOCK_NODE *pNode, *pPreNode;

			////////////////////////////////////////////////////////////////
			// 메모리 풀 크기 설정
			////////////////////////////////////////////////////////////////
			m_iBlockCount = iBlockNum;

			if (iBlockNum < 0)	return;

			else if (iBlockNum == 0)
			{
				m_bStoreFlag = true;
				m_stpTop = NULL;
			}

			else
			{
				m_bStoreFlag = false;

				pNode = (st_BLOCK_NODE *)malloc(sizeof(DATA) + sizeof(st_BLOCK_NODE));
				m_stpTop = pNode;
				pPreNode = pNode;

				for (int iCnt = 1; iCnt < iBlockNum; iCnt++)
				{
					pNode = (st_BLOCK_NODE *)malloc(sizeof(DATA) + sizeof(st_BLOCK_NODE));
					pPreNode->stpNextBlock = pNode;
					pPreNode = pNode;
				}
			}

			InitializeSRWLock(&srwMemLock);
		}

		virtual	~CMemoryPool()
		{
			st_BLOCK_NODE *pNode;
			for (int iCnt = 0; iCnt < m_iBlockCount; iCnt++)
			{
				pNode = m_stpTop;
				m_stpTop = m_stpTop->stpNextBlock;
				free(pNode);
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// 블럭 하나를 할당받는다.
		//
		// Parameters: 없음.
		// Return: (DATA *) 데이타 블럭 포인터.
		//////////////////////////////////////////////////////////////////////////
		DATA	*Alloc(bool bPlacementNew = true)
		{
			st_BLOCK_NODE *stpBlock;
			int iBlockCount = m_iBlockCount;
			InterlockedIncrement64((LONG64 *)&m_iAllocCount);
		
			if (iBlockCount < m_iAllocCount)
			{
				if (m_bStoreFlag)
				{
					stpBlock = (st_BLOCK_NODE *)malloc(sizeof(DATA) + sizeof(st_BLOCK_NODE));
					InterlockedIncrement64((LONG64 *)&m_iBlockCount);
				}

				else		return nullptr;
			}

			else
			{
				stpBlock = m_stpTop;
				m_stpTop = stpBlock->stpNextBlock;
			}

			return (DATA *)(stpBlock + 1);
		}

		//////////////////////////////////////////////////////////////////////////
		// 사용중이던 블럭을 해제한다.
		//
		// Parameters: (DATA *) 블럭 포인터.
		// Return: (BOOL) TRUE, FALSE.
		//////////////////////////////////////////////////////////////////////////
		bool	Free(DATA *pData)
		{
			st_BLOCK_NODE *stpBlock;

			stpBlock = ((st_BLOCK_NODE *)pData - 1);
			stpBlock->stpNextBlock = m_stpTop;

			m_stpTop = stpBlock;
			InterlockedDecrement64((LONG64 *)&m_iAllocCount);
			return false;
		}


		//////////////////////////////////////////////////////////////////////////
		// 현재 사용중인 블럭 개수를 얻는다.
		//
		// Parameters: 없음.
		// Return: (int) 사용중인 블럭 개수.
		//////////////////////////////////////////////////////////////////////////
		int		GetAllocCount(void) { return m_iAllocCount; }


		void		Lock()
		{
			AcquireSRWLockExclusive(&srwMemLock);
		}

		void		Unlock()
		{
			ReleaseSRWLockExclusive(&srwMemLock);
		}

	private:
		//////////////////////////////////////////////////////////////////////////
		// 블록 스택의 탑
		//////////////////////////////////////////////////////////////////////////
		st_BLOCK_NODE *m_stpTop;

		//////////////////////////////////////////////////////////////////////////
		// 메모리 Lock 플래그
		//////////////////////////////////////////////////////////////////////////
		bool m_bLockFlag;

		//////////////////////////////////////////////////////////////////////////
		// 메모리 동적 플래그, true면 없으면 동적할당 함
		//////////////////////////////////////////////////////////////////////////
		bool m_bStoreFlag;

		//////////////////////////////////////////////////////////////////////////
		// 현재 사용중인 블럭 개수
		//////////////////////////////////////////////////////////////////////////
		int m_iAllocCount;

		//////////////////////////////////////////////////////////////////////////
		// 전체 블럭 개수
		//////////////////////////////////////////////////////////////////////////
		int m_iBlockCount;

		//////////////////////////////////////////////////////////////////////////
		// SRWLOCK
		//////////////////////////////////////////////////////////////////////////
		SRWLOCK srwMemLock;
	};

#endif