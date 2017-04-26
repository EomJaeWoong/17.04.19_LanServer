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
			////////////////////////////////////////////////////////////////
			// �޸� Ǯ ũ�� ����
			////////////////////////////////////////////////////////////////
			m_iBlockCount = iBlockNum;

			if (iBlockNum < 0)	return;

			else if (iBlockNum == 0)
			{
				m_bStoreFlag = true;
				m_stBlockHeader = NULL;
			}

			////////////////////////////////////////////////////////////////
			// DATA * ���� ũ�⸸ ŭ �޸� �Ҵ�
			////////////////////////////////////////////////////////////////
			m_stBlockHeader = new char[(sizeof(DATA) + sizeof(st_BLOCK_NODE)) * m_iBlockCount];

			m_stpTop = (st_BLOCK_NODE *)m_stBlockHeader;
			char *pBlock = (char *)m_stpTop;
			st_BLOCK_NODE *stpNode = m_stpTop;

			////////////////////////////////////////////////////////////////
			// BLOCK ����
			////////////////////////////////////////////////////////////////
			for (int iCnt = 0; iCnt < m_iBlockCount - 1; iCnt++)
			{
				pBlock += sizeof(DATA) + sizeof(st_BLOCK_NODE);
				stpNode->stpNextBlock = (st_BLOCK_NODE *)pBlock;
				stpNode = stpNode->stpNextBlock;
			}

			stpNode->stpNextBlock = NULL;
		}

		virtual	~CMemoryPool()
		{
			delete []m_stBlockHeader;
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

			if (m_iBlockCount < m_iAllocCount)		return NULL;

			else if (m_bStoreFlag && (m_iBlockCount == m_iAllocCount))
			{
				stpBlock = (st_BLOCK_NODE *)new char[(sizeof(st_BLOCK_NODE) + sizeof(DATA))];
				m_iBlockCount++;
			}

			else
			{
				stpBlock = m_stpTop;
				m_stpTop = stpBlock->stpNextBlock;
			}

			m_iAllocCount++;

			return (DATA *)stpBlock + 1;
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
			return false;
		}


		//////////////////////////////////////////////////////////////////////////
		// ���� ������� �� ������ ��´�.
		//
		// Parameters: ����.
		// Return: (int) ������� �� ����.
		//////////////////////////////////////////////////////////////////////////
		int		GetAllocCount(void) { return m_iAllocCount; }

	private:
		//////////////////////////////////////////////////////////////////////////
		// ��� ������ ž
		//////////////////////////////////////////////////////////////////////////
		st_BLOCK_NODE *m_stpTop;

		//////////////////////////////////////////////////////////////////////////
		// ��� ����ü ���
		//////////////////////////////////////////////////////////////////////////
		char *m_stBlockHeader;

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
	};

#endif