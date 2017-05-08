/*---------------------------------------------------------------

	procademy MemoryPool.

	�޸� Ǯ Ŭ����.
	Ư�� ����Ÿ(����ü,Ŭ����,����)�� ������ �Ҵ� �� ��������.

	- ����.

	procademy::CMemoryPool<DATA> MemPool(300, FALSE);
	DATA *pData = MemPool.Alloc();

	pData ���

	MemPool.Free(pData);


	!.	���� ���� ���Ǿ� �ӵ��� ������ �� �޸𸮶�� �����ڿ���
		Lock �÷��׸� �־� ����¡ ���Ϸ� ���縦 ���� �� �ִ�.
		���� �߿��� ��찡 �ƴ��̻� ��� ����.

		
		
		���ǻ��� :	�ܼ��� �޸� ������� ����Ͽ� �޸𸮸� �Ҵ��� �޸� ����� �����Ͽ� �ش�.
					Ŭ������ ����ϴ� ��� Ŭ������ ������ ȣ�� �� Ŭ�������� �Ҵ��� ���� ���Ѵ�.
					Ŭ������ �����Լ�, ��Ӱ��谡 ���� �̷����� �ʴ´�.
					VirtualAlloc ���� �޸� �Ҵ� �� memset ���� �ʱ�ȭ�� �ϹǷ� Ŭ���������� ���� ����.
		
				
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
		// �� �� �տ� ���� ��� ����ü.
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
		// ������, �ı���.
		//
		// Parameters:	(int) �ִ� �� ����.
		//				(bool) �޸� Lock �÷��� - �߿��ϰ� �ӵ��� �ʿ�� �Ѵٸ� Lock.
		// Return:
		//////////////////////////////////////////////////////////////////////////
		CMemoryPool(int iBlockNum, bool bLockFlag = false)
		{
			st_BLOCK_NODE *pNode, *pPreNode;

			////////////////////////////////////////////////////////////////
			// �޸� Ǯ ũ�� ����
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
		// �� �ϳ��� �Ҵ�޴´�.
		//
		// Parameters: ����.
		// Return: (DATA *) ����Ÿ �� ������.
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
		// ������̴� ���� �����Ѵ�.
		//
		// Parameters: (DATA *) �� ������.
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
		// ���� ������� �� ������ ��´�.
		//
		// Parameters: ����.
		// Return: (int) ������� �� ����.
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
		// ��� ������ ž
		//////////////////////////////////////////////////////////////////////////
		st_BLOCK_NODE *m_stpTop;

		//////////////////////////////////////////////////////////////////////////
		// �޸� Lock �÷���
		//////////////////////////////////////////////////////////////////////////
		bool m_bLockFlag;

		//////////////////////////////////////////////////////////////////////////
		// �޸� ���� �÷���, true�� ������ �����Ҵ� ��
		//////////////////////////////////////////////////////////////////////////
		bool m_bStoreFlag;

		//////////////////////////////////////////////////////////////////////////
		// ���� ������� �� ����
		//////////////////////////////////////////////////////////////////////////
		int m_iAllocCount;

		//////////////////////////////////////////////////////////////////////////
		// ��ü �� ����
		//////////////////////////////////////////////////////////////////////////
		int m_iBlockCount;

		//////////////////////////////////////////////////////////////////////////
		// SRWLOCK
		//////////////////////////////////////////////////////////////////////////
		SRWLOCK srwMemLock;
	};

#endif