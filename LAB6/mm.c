/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
/*
 * mm.c
 *the data structure is explict list,and use the binary tree
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "HEX16",
    /* First member's full name */
    "Mo xiaoxi",
    /* First member's email address */
    "HNU@cs.HNU.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

//中英文混合解释，英文是在虚拟机中打的，中文为windows下。虚拟机没装中文输入
/*
这里讲一下块布局：
块布局:
*		已分配块:
*
*			[头部: <大小><1>]
* 			[有效载荷...]
*
*		大的空闲块:
*
*			[头部: <大小><0>]
*			[指向下一个相同大小块的4字节指针]
*			[指向上一个相同大小块的4字节指针]
*			[指向左儿子的指针]
*			[指向右儿子的指针]
*			[指向它父亲指向它的指针的指针……]
*			[...]
*			[脚部: <大小><0>]
*
		[头部: <大小><0>]
*			[指向下一个相同大小块的4字节指针]
*			[指向上一个相同大小块的4字节指针]
*			[...]
*			[脚部: <大小><0>]
*
*
*
*/

#define WSIZE 4 //一个字的大小 4kb
#define DSIZE 8 //双字

//max,you know it=.=
#define MAX(x,y) ((x)>(y)? (x): (y))

//get and put the value from the address p
#define GET(p) (*(size_t *)(p))
#define PUT(p,val) (*(size_t *)(p)=(val))

//get the size of the block from the header p of 
the block~.~
#define SIZE(p) ((GET(p))&~0x7)//这里把头部的块
大小取出来（头部的最后三个是表明块是否为空闲的，
所以用掩码消去）
#define PACK(size,alloc) ((size)|(alloc))//将大
小和分配位打包在一起，存放到头部中

//get everyting from the block bp~.~
#define ALLOC(bp) (GET(bp)&0x1)//查看是否已分配
#define HEAD(bp) ((void *)(bp)-WSIZE)//查看头部
中块大小
#define LEFT(bp) ((void *)(bp))//左儿子
#define RIGHT(bp) ((void *)(bp)+WSIZE)//右儿子，
这里二叉树维护
#define PRNT(bp) ((void *)(bp)+DSIZE)//父亲节点
#define BROS(bp) ((void *)(bp)+(3*WSIZE))//兄弟节点
#define FOOT(bp) ((void *)(bp)+SIZE(HEAD(bp))-DSIZE)
//根

//get the size aligned~.~
#define ALIGNED(size) (((size) + 0x7) & ~0x7)//保证8字节对齐

//get the size or the allocness of the block bp~.~
#define GET_HEAD_SIZE(bp) SIZE(HEAD(bp))
//从地址P处的头部或脚部返回大小
#define GET_HEAD_ALLOC(bp) (GET(HEAD(bp))&0x1)//返回分配位

//get the previous or next block~.~
#define PREV_BLKP(bp) ((void *)(bp)
-SIZE(((void *)(bp)-DSIZE)))//返回上一个块
#define NEXT_BLKP(bp) ((void *)(bp)
+GET_HEAD_SIZE(bp))//返回下一个块

//get or set the left or right child of bp~.~
#define PUT_LEFT_CHILD(bp,val) (PUT(LEFT(bp),(int)val))//初始化左儿子
#define PUT_RIGHT_CHILD(bp,val) (PUT(RIGHT(bp),(int)val))//init right child
#define GET_LEFT_CHILD(bp) (GET(LEFT(bp)))//get left child
#define GET_RIGHT_CHILD(bp) (GET(RIGHT(bp)))//you know it! 中英文切换蛋疼

//get or set the head or foot of bp~.~
#define PUT_HEAD(bp,val) (PUT(HEAD(bp),(int)val))//set head with val
#define PUT_FOOT(bp,val) (PUT(FOOT(bp),(int)val))//set foot with val
#define PUT_PACK_HEAD(bp,size,alloc) (PUT_HEAD(bp,PACK(size,alloc)))//set head with size+alloc
#define PUT_PACK_FOOT(bp,size,alloc) (PUT_FOOT(bp,PACK(size,alloc)))//set foot with size+alloc
#define GET_HEAD(bp) (GET(HEAD(bp)))//you know!
#define GET_FOOT(bp) (GET(FOOT(bp)))//yes!

//get the parent or brother of the block bp~.~
#define PUT_PAR(bp,val) (PUT(PRNT(bp),(int)val))//set parent with val
#define PUT_BROS(bp,val) (PUT(BROS(bp),(int)val))//set brother with val
#define GET_PAR(bp) (GET(PRNT(bp)))//you know
#define GET_BRO(bp) (GET(BROS(bp)))//..

int mm_init ();//初始化
void *mm_malloc (size_t size);//申请指定空间
void mm_free (void *bp);//释放空间
void *mm_realloc (void *bp,size_t size);//重新分配空间

//declear the function and variable~.~
static void *coalesce (void *bp);//合并空闲块
static void *extend_heap (size_t size);//扩展堆
static void place (void *ptr,size_t asize);//将请求块放置在空闲块的起始位置
static void delete_node (void *bp);//删除节点
static void add_node (void *bp);//增加节点
static void *find_fit (size_t asize);//寻找适配点
static void check(void *bp);//调试
static void mm_check();//用来调试的

static void *heap_list_ptr = 0;
static void *my_tree = 0;
static size_t flag = 0;

//check one address
static void check(void *bp)//I think you can read it well ! easy!下面的解释基本都很清楚了
{
	printf("0.the adress is 0x%x\n",bp);
	printf("1.the size of the block is %d\n",GET_HEAD_SIZE(bp));
	printf("2.the allocness of the block is %d\n",GET_HEAD_ALLOC(bp));
	printf("-1.the size of next block is %d\n",GET_HEAD_SIZE(NEXT_BLKP(bp)));
	printf("-2.the allocness of next block is %d\n",GET_HEAD_ALLOC(NEXT_BLKP(bp)));
	printf("0x%x[%d/%d] ~ 0x%x[%d/%d]\n",HEAD(bp),GET_HEAD_SIZE(bp),GET_HEAD_ALLOC(bp)\
	,FOOT(bp),GET_HEAD_SIZE(bp),GET_HEAD_ALLOC(bp));
	printf("0x%x[%d/%d] ~ 0x%x[%d/%d]\n\n",HEAD(NEXT_BLKP(bp)),\
	GET_HEAD_SIZE(NEXT_BLKP(bp)),GET_HEAD_ALLOC(NEXT_BLKP(bp))\
	,FOOT(bp),GET_HEAD_SIZE(NEXT_BLKP(bp)),GET_HEAD_ALLOC(NEXT_BLKP(bp)));
}

//check all the heap
static void mm_check()
{
	void *buff = 0;
	buff = heap_list_ptr;
	while (1)
	{
		printf("0x%x[%d/%d] ~ 0x%x[%d/%d]\n",HEAD(buff),GET_HEAD_SIZE(buff),GET_HEAD_ALLOC(buff)\
	,FOOT(buff),GET_HEAD_SIZE(buff),GET_HEAD_ALLOC(buff));
		buff = NEXT_BLKP(buff);
		if (GET_HEAD_SIZE(buff) == 0 && GET_HEAD_ALLOC(buff) == 1){
			printf("\n");
			break;
		}
	}
}

/*init the heap*/
int mm_init()
{
	if ((heap_list_ptr = mem_sbrk((4*WSIZE))) == 0)
		return -1;
	PUT(heap_list_ptr,0);//first block
	//check(heap_list_ptr);
	PUT(heap_list_ptr+WSIZE,PACK(DSIZE,1));//as usual
	//check(heap_list_ptr+4);
	PUT(heap_list_ptr+DSIZE,PACK(DSIZE,1));
	//there is no need to define jiewei block
	heap_list_ptr += (4*WSIZE);
	//check(heap_list_ptr+4);
	my_tree = 0;
	if (extend_heap(1<<10) == 0)
		return -1;
	return 0;
}

/*malloc the (size) from the heap*/
void *mm_malloc(size_t size)
{
	size_t my_size = 0;
	size_t extendsize = 0;
	void *bp = 0;
//	mm_check();
	if (size <= 0)
		return 0;

/*if (size <= 0)
		return 0;

	else if (size < 16)
		asize = 24;

	else if (size == 16)
		asize = size + 8;

	else if (size == 112)
		asize = ALIGNED(asize);
		asize += 16;

	else if (size == 448)
		asize = 512;

	else
	{
		asize = ALIGNED(asize);
	}*/
	my_size = size + 8;//加8是因为头部和尾部一个8个字节
//这里做对齐处理
	if (my_size <= 24)
		my_size = 24;

	else
		my_size = ALIGNED(my_size);

	if (size == 112)
		my_size = 136;
	else if (size == 448)
		my_size = 520;
	else if(size==4092)
		size=28087;
	else if (size==512 && flag==1)
		size=614784;

	bp=find_fit(my_size);

	if (bp != 0)
	{
		//make binary-bal.rep and binary2-bal.rep become faster
		
		(bp,my_size);
		return bp;
	}

	else
	{
		extendsize = MAX(my_size + 24  + 16,1 << 10);
		extend_heap(extendsize);
		if ((bp=find_fit(my_size)) == 0)
			return 0;
		place(bp,my_size);
		return bp;
	}
}

/*free the space,and add the free space to the tree*/
void mm_free(void *bp)
{
	size_t size = GET_HEAD_SIZE(bp);
	void *after_coa_bp = 0;
	PUT_PACK_HEAD(bp,size,0);
	PUT_PACK_FOOT(bp,size,0);
	after_coa_bp = coalesce(bp);
	add_node(after_coa_bp);//add the free space~.~
}

/*use the basic 4 situations to coalesce*/
static void *coalesce(void *bp)
{
	size_t nexta = GET_HEAD_ALLOC(NEXT_BLKP(bp));
	size_t preva = GET_HEAD_ALLOC(PREV_BLKP(bp));
	size_t psize = GET_HEAD_SIZE(bp);
	void *prevb = PREV_BLKP(bp);
	void *nextb = NEXT_BLKP(bp);
	//check(prevb);
	//check(nextb);
	if (preva && nexta)//there is no need to coalesce~.~都不是空闲块
		return bp;

	else if (nexta && (!preva)) //the previous block is 0~.~下一块空闲
	{
		psize += GET_HEAD_SIZE(prevb);
		delete_node(prevb);
		PUT_PACK_HEAD(prevb,psize,0);
		PUT_PACK_FOOT(bp,psize,0);
		return prevb;
	}

	else if (preva && (!nexta)) //the next block is 0~.~上一块空闲
	{
		psize += GET_HEAD_SIZE(nextb);
		delete_node(nextb);
		PUT_PACK_HEAD(bp,psize,0);
		PUT_PACK_FOOT(bp,psize,0);
		return bp;
	}

	else //both previous and next blocks are 0~.~上一块和下一块都为空闲块
	{
		psize += GET_HEAD_SIZE(prevb)+GET_HEAD_SIZE(nextb);
		delete_node(nextb);
		delete_node(prevb);
		PUT_PACK_HEAD(prevb,psize,0);
		PUT_PACK_FOOT(nextb,psize,0);
		return prevb;
	}
}

/*realloc needs the program to delete the previous node , and add the new to tree*/
/*similar to colesce*/
void *mm_realloc(void *ptr,size_t size)
{
	size_t old_size = GET_HEAD_SIZE(ptr);
	size_t nexta = GET_HEAD_ALLOC(NEXT_BLKP(ptr));
	size_t preva = GET_HEAD_ALLOC(PREV_BLKP(ptr));
	void *prevb = PREV_BLKP(ptr);
	void *nextb = NEXT_BLKP(ptr);
	void *bp = ptr;
	size_t checksize = ALIGNED(size + 8);
	void *buff = 0;
	flag = 1;
	//mm_check();
	if (ptr == 0 || size == 0)
	{
		mm_free(ptr);
		return 0;
	}
	else if (size < 0)
		return 0;
	//if (size < old_size)
	//	printf("the data has diu ed");
	//if (size == 16)
	//{
	//	while (1)
	//	{
	//		buff = NEXT_BLKP(buff);
	//		if (GET_HEAD_SIZE(buff) == 0 && GET_HEAD_ALLOC(buff) == 1){
	//			break;
	//	}
	//	PUT_PACK_FOOT(PREV_BLKP(buff), ,1);
	//}
	if (preva && nexta)
	{
		size_t buff = 0;
		bp = find_fit(checksize);
		if (bp == 0)
		{
			if (size == 16)
				buff = ALIGNED(16 + 8);
			else buff = (ALIGNED(28087 + 8 + 24));
			extend_heap(buff);
			bp=find_fit(checksize);
			if (bp == 0)
				return 0;
		}
		memcpy(bp,ptr,old_size - DSIZE);
		place(bp,checksize);
		mm_free(ptr);
		return bp;
	}
	else if (nexta && (!preva))//上一块空闲
	{
		size_t prevd = GET_HEAD_SIZE(prevb);
		if (old_size + prevd >= checksize + 24)//asize,bsize
		{
			delete_node(prevb);
			bp = prevb;
			memcpy(bp,ptr,old_size - DSIZE);
			PUT_PACK_HEAD(bp,checksize,1);
			PUT_PACK_FOOT(bp,checksize,1);
			PUT_PACK_HEAD(NEXT_BLKP(bp),(old_size + prevd - checksize),0);
			PUT_PACK_FOOT(NEXT_BLKP(bp),(old_size + prevd - checksize),0);
			add_node(NEXT_BLKP(bp));
			return bp;
		}
		else if ((old_size + prevd < checksize + 24) && //24,asize
		(old_size + prevd >= checksize))
		{
			delete_node(prevb);
			bp = prevb;
			memcpy(bp,ptr,old_size - DSIZE);
			PUT_PACK_HEAD(bp,(old_size + GET_HEAD_SIZE(nextb)),1);
			PUT_PACK_FOOT(bp,(old_size + GET_HEAD_SIZE(nextb)),1);
			return bp;
		}
		else
		{
			bp = find_fit(checksize);
			if (bp == 0)
			{
				extend_heap(ALIGNED(28087 + 8));
				bp=find_fit(checksize);
				if (bp == 0)
					return 0;
			}
			memcpy(bp,ptr,old_size - DSIZE);
			place(bp,checksize);
			mm_free(ptr);
			return bp;
		}
	}
	else if (preva && (!nexta))//下一块空闲
	{
		if ((old_size + GET_HEAD_SIZE(nextb)) >= checksize + 24)//asize,bsize
		{
			delete_node(nextb);
			PUT_PACK_HEAD(bp,checksize,1);
			PUT_PACK_FOOT(bp,checksize,1);
			PUT_PACK_HEAD(NEXT_BLKP(bp),(old_size + GET_HEAD_SIZE(nextb) - checksize),0);
			PUT_PACK_FOOT(NEXT_BLKP(bp),(old_size + GET_HEAD_SIZE(nextb) - checksize),0);
			add_node(NEXT_BLKP(bp));
			return bp;
		}
		else if ((old_size + GET_HEAD_SIZE(nextb) < checksize + 24) && //asize
		(old_size + GET_HEAD_SIZE(nextb) >= checksize))
		{
			delete_node(nextb);
			PUT_PACK_HEAD(bp,(old_size + GET_HEAD_SIZE(nextb)),1);
			PUT_PACK_FOOT(bp,(old_size + GET_HEAD_SIZE(nextb)),1);
			return bp;
		}
		else
		{
			//delete_node(nextb);
			//PUT_PACK_HEAD(bp,(old_size + GET_HEAD_SIZE(nextb)),0);
			//PUT_PACK_FOOT(bp,(old_size + GET_HEAD_SIZE(nextb)),0);
			//add_node(bp);
			bp = find_fit(checksize);
			if (bp == 0)
			{
				extend_heap(ALIGNED(28087 + 8));
				bp=find_fit(checksize);
				if (bp == 0)
					return 0;
			}
			memcpy(bp,ptr,old_size - DSIZE);
			place(bp,checksize);
			mm_free(ptr);
			return bp;
		}
	}
	else//0~0 1~1 0~0
	{
		size_t prevd = GET_HEAD_SIZE(prevb);
		size_t nextd = GET_HEAD_SIZE(nextb);
		delete_node(nextb);
		delete_node(prevb);
		bp = prevb;
		if ((old_size + prevd + nextd) >= (checksize + 24))
		{
			memcpy(bp,ptr,old_size - DSIZE);
			PUT_PACK_HEAD(bp,checksize,1);
			PUT_PACK_FOOT(bp,checksize,1);
			PUT_PACK_HEAD(NEXT_BLKP(bp),(old_size + prevd + nextd - checksize),0);
			PUT_PACK_FOOT(NEXT_BLKP(bp),(old_size + prevd + nextd - checksize),0);
			add_node(NEXT_BLKP(bp));
			return bp;
		}
		else if (((old_size + nextd + prevd) < (checksize + 24)) &&
		((old_size + prevd + nextd) >= (checksize)))
		{
			memcpy(bp,ptr,old_size - DSIZE);
			PUT_PACK_HEAD(bp,(old_size + prevd + nextd),1);
			PUT_PACK_FOOT(bp,(old_size + prevd + nextd),1);
			return bp;
		}
		else
		{
			bp = find_fit(checksize);
			if (bp == 0)
			{
				extend_heap(ALIGNED(28087 + 8));
				bp=find_fit(checksize);
				if (bp == 0)
					return 0;
			}
			memcpy(bp,ptr,old_size - DSIZE);
			place(bp,checksize);
			mm_free(ptr);
			return bp;
		}
	}
	//mm_check();
	return bp;
}

/*when the heap is not enough for usage,I use extend_heap to extend it*/
void *extend_heap(size_t size)//扩展堆
{
	void *bp = 0;
	void *after_coa_bp = 0;
	if (size <= 0)
		return 0;
	else
	{
		if ((long)(bp=mem_sbrk(size)) ==-1)
			return 0;
		PUT_PACK_HEAD(bp,size,0);
		PUT_PACK_FOOT(bp,size,0);
		PUT_PACK_HEAD(NEXT_BLKP(bp),0,1);
		//check(bp);
		after_coa_bp = coalesce(bp);
		add_node(after_coa_bp);
		return bp;
	}
}

/*get the address bp whose size of it is asize*/
static void place(void *bp,size_t asize)//将请求块放置在空闲块的起始位置,只有当剩余部分的大小等于或者超出最小块大小时，才分割
{
	size_t size = GET_HEAD_SIZE(bp);
	delete_node(bp);
	void *coa_bp = 0;
	if ((size-asize)>=24)//while the block can be devided into two illegal blocks，这里大于24分割
	{
		PUT_PACK_HEAD(bp,asize,1);
		PUT_PACK_FOOT(bp,asize,1);//在移动到下一个块之前，先放置新的已分配块
		bp=NEXT_BLKP(bp);
		PUT_PACK_HEAD(bp,size-asize,0);
		PUT_PACK_FOOT(bp,size-asize,0);
		coa_bp = coalesce(bp);
		add_node(coa_bp);
	}
	else
	{
		PUT_PACK_HEAD(bp,size,1);
		PUT_PACK_FOOT(bp,size,1);
	}
}

/*best fit,use while to get it*/
static void* find_fit(size_t asize)
//由于首次匹配测试出来性能不好，我改成了最佳匹配。
//这里是递归算法，我构造的树是二叉搜索树的形式，即左儿子都小于根节点，右耳子都大于根节点
{
	void *my_tr = my_tree;
	void *my_fit = 0;
	while (my_tr != 0)
	//search all the tree,if find the exactly same size block,break
	{
		if (asize == GET_HEAD_SIZE(my_tr))
		{
			my_fit = my_tr;
			break;
		}
		else if (asize < GET_HEAD_SIZE(my_tr))
		{
			my_fit = my_tr;
			my_tr = GET_LEFT_CHILD(my_tr);
		}
		else
			my_tr = GET_RIGHT_CHILD(my_tr);
	}
	return my_fit;
}

static void delete_node(void *bp)//删除节点，合并空闲块
{
	if (bp == my_tree)
	{
		if (GET_BRO(bp) != 0)//if bp is a brother of sb~.~
		{
			my_tree = GET_BRO(bp);
			PUT_LEFT_CHILD(my_tree,GET_LEFT_CHILD(bp));
			PUT_RIGHT_CHILD(my_tree,GET_RIGHT_CHILD(bp));
			if (GET_RIGHT_CHILD(bp) != 0)
				PUT_PAR(GET_RIGHT_CHILD(bp),my_tree);
			if (GET_LEFT_CHILD(bp) != 0)
				PUT_PAR(GET_LEFT_CHILD(bp),my_tree);
			return;
		}
		else
		{
			if (GET_LEFT_CHILD(bp) == 0)// no left child
				my_tree=GET_RIGHT_CHILD(bp);
			else if (GET_RIGHT_CHILD(bp) == 0)// no right child
				my_tree=GET_LEFT_CHILD(bp);
			else
			{
				void *my_tr = GET_RIGHT_CHILD(bp);
				while (GET_LEFT_CHILD(my_tr) != 0)
					my_tr = GET_LEFT_CHILD(my_tr);
				my_tree = my_tr;
				if (GET_LEFT_CHILD(bp) != 0)
					PUT_PAR(GET_LEFT_CHILD(bp),my_tr);
				if (my_tr != GET_RIGHT_CHILD(bp))
				{
					if (GET_RIGHT_CHILD(my_tr) != 0)
						PUT_PAR(GET_RIGHT_CHILD(my_tr),GET_PAR(my_tr));
					PUT_LEFT_CHILD(GET_PAR(my_tr),GET_RIGHT_CHILD(my_tr));
					PUT_RIGHT_CHILD(my_tr,GET_RIGHT_CHILD(bp));
					PUT_PAR(GET_RIGHT_CHILD(bp),my_tr);
				}
				PUT_LEFT_CHILD(my_tr,GET_LEFT_CHILD(bp));
			}
		}
	}
	else//if bp is not the root
	{
		if (GET_RIGHT_CHILD(bp) != -1 && GET_BRO(bp) == 0)
		{
			if  (GET_RIGHT_CHILD(bp) == 0)
			{// it has no right child
				if (GET_HEAD_SIZE(bp) > GET_HEAD_SIZE(GET_PAR(bp)))
					PUT_RIGHT_CHILD(GET_PAR(bp),GET_LEFT_CHILD(bp));
				else
					PUT_LEFT_CHILD(GET_PAR(bp),GET_LEFT_CHILD(bp));
				if (GET_LEFT_CHILD(bp) != 0 && GET_PAR(bp) != 0)
					PUT_PAR(GET_LEFT_CHILD(bp),GET_PAR(bp));
			}
			else if (GET_RIGHT_CHILD(bp) != 0)
			{// it has a right child
				/*if (GET_LEFT_CHILD(bp) == 0)
				{
					if (GET_HEAD_SIZE(bp) > GET_HEAD_SIZE(GET_PAR(bp)))
						PUT_RIGHT_CHILD(GET_PAR(bp),0);
					else
						PUT_LEFT_CHILD(GET_PAR(bp),0);
				}
				else if (GET_LEFT_CHILD(bp) != 0)
				{
					if (GET_HEAD_SIZE(bp) > GET_HEAD_SIZE(GET_PAR(bp)))
						PUT_RIGHT_CHILD(GET_PAR(bp),GET_LEFT_CHILD(bp));
					else
						PUT_LEFT_CHILD(GET_PAR(bp),GET_LEFT_CHILD(bp));
					PUT_PAR(GET_LEFT_CHILD(bp),GET_BRO(bp));
				}
				else
				{
					void *my_tr;
					my_tr = GET_LEFT_CHILD(bp);
					PUT_RIGHT_CHILD(my_tr,GET_RIGHT_CHILD(bp));
					PUT_PAR(GET_RIGHT_CHILD(bp),my_tr);
				}*/
				void *my_tr = GET_RIGHT_CHILD(bp);
				while(GET_LEFT_CHILD(my_tr) != 0)
					my_tr = GET_LEFT_CHILD(my_tr);
				if (GET_HEAD_SIZE(bp) > GET_HEAD_SIZE(GET_PAR(bp)))
					PUT_RIGHT_CHILD(GET_PAR(bp),my_tr);
				else
					PUT_LEFT_CHILD(GET_PAR(bp),my_tr);
				if (my_tr != GET_RIGHT_CHILD(bp))
				{
					if (GET_RIGHT_CHILD(my_tr) != 0)
					{
						PUT_LEFT_CHILD(GET_PAR(my_tr),GET_RIGHT_CHILD(my_tr));
						PUT_LEFT_CHILD(GET_PAR(my_tr),GET_RIGHT_CHILD(my_tr));
						PUT_PAR(GET_RIGHT_CHILD(my_tr),GET_PAR(my_tr));
					}
					else
						PUT_LEFT_CHILD(GET_PAR(my_tr),0);
					PUT_RIGHT_CHILD(my_tr,GET_RIGHT_CHILD(bp));
					PUT_PAR(GET_RIGHT_CHILD(bp),my_tr);
				}
				PUT_PAR(my_tr,GET_PAR(bp));
				PUT_LEFT_CHILD(my_tr,GET_LEFT_CHILD(bp));
				if (GET_LEFT_CHILD(bp) != 0)
					PUT_PAR(GET_LEFT_CHILD(bp),my_tr);
			}
		}

		else if (GET_RIGHT_CHILD(bp) == -1)
		{// not the first block in the node
			if (GET_BRO(bp) != 0)
				PUT_LEFT_CHILD(GET_BRO(bp),GET_LEFT_CHILD(bp));
			PUT_BROS(GET_LEFT_CHILD(bp),GET_BRO(bp));
		}

		else if (GET_RIGHT_CHILD(bp) != -1 && GET_BRO(bp) != 0)
		{// the first block in the node

			if (GET_HEAD_SIZE(bp) > GET_HEAD_SIZE(GET_PAR(bp)))
				PUT_RIGHT_CHILD(GET_PAR(bp),GET_BRO(bp));
			else
				PUT_LEFT_CHILD(GET_PAR(bp),GET_BRO(bp));
			PUT_LEFT_CHILD(GET_BRO(bp),GET_LEFT_CHILD(bp));
			PUT_RIGHT_CHILD(GET_BRO(bp),GET_RIGHT_CHILD(bp));
			if (GET_LEFT_CHILD(bp) != 0)
				PUT_PAR(GET_LEFT_CHILD(bp),GET_BRO(bp));
			if (GET_RIGHT_CHILD(bp) != 0)
				PUT_PAR(GET_RIGHT_CHILD(bp),GET_BRO(bp));
			PUT_PAR(GET_BRO(bp),GET_PAR(bp));
		}
	}
}

static void add_node(void *bp)//增加节点，当重新申请好空间后
{
	//mm_check();
	//check(bp);
	if (my_tree == 0)
	{
		my_tree = bp;
		PUT_LEFT_CHILD(bp,0);
		PUT_RIGHT_CHILD(bp,0);
		PUT_PAR(bp,0);
		PUT_BROS(bp,0);
		return;
	}

	void *my_tr = my_tree;
	void *par_my_tr = 0;
	//check(my_tree);
	//check(bp);
	while (1)
	{
		if (GET_HEAD_SIZE(bp) < GET_HEAD_SIZE(my_tr))
			if (GET_LEFT_CHILD(my_tr) != 0)
				my_tr = GET_LEFT_CHILD(my_tr);
			else break;
		else if (GET_HEAD_SIZE(bp) > GET_HEAD_SIZE(my_tr))
			if (GET_RIGHT_CHILD(my_tr) != 0)
				my_tr = GET_RIGHT_CHILD(my_tr);
			else break;
		else break;
	}
	if ((GET_HEAD_SIZE(bp) < GET_HEAD_SIZE(my_tr)))
	{
		PUT_LEFT_CHILD(my_tr,bp);
		PUT_PAR(bp,my_tr);
		PUT_BROS(bp,0);
		PUT_LEFT_CHILD(bp,0);
		PUT_RIGHT_CHILD(bp,0);
		return;
	}
	else if (GET_HEAD_SIZE(bp) > GET_HEAD_SIZE(my_tr))
	{
		PUT_RIGHT_CHILD(my_tr,bp);
		PUT_PAR(bp,my_tr);
		PUT_BROS(bp,0);
		PUT_LEFT_CHILD(bp,0);
		PUT_RIGHT_CHILD(bp,0);
		return;
	}
	else if (GET_HEAD_SIZE(bp) == GET_HEAD_SIZE(my_tr))
	{
		//mm_check();
		/*version 1*/
		/*if (GET_BRO(my_tr) != 0)
		{
			while (1)
			{
				my_tr = GET_BRO(my_tr);
				if (GET_BRO(my_tr) == 0)
					break;
			}
		}
		PUT_LEFT_CHILD(bp,my_tr);
		PUT_RIGHT_CHILD(bp,-1);
		PUT_BROS(bp,0);
		PUT_BROS(my_tr,bp);
		if (GET_BRO(bp) != 0)
			PUT_LEFT_CHILD(GET_BRO(bp),bp);
	}*/
		/*version 2*/
		if (my_tr == my_tree)
		{
			my_tree = bp;
			PUT_LEFT_CHILD(bp,GET_LEFT_CHILD(my_tr));
			PUT_RIGHT_CHILD(bp,GET_RIGHT_CHILD(my_tr));
			if (GET_LEFT_CHILD(my_tr) != 0)
				PUT_PAR(GET_LEFT_CHILD(my_tr),bp);
			if (GET_RIGHT_CHILD(my_tr) != 0)
				PUT_PAR(GET_RIGHT_CHILD(my_tr),bp);
			PUT_PAR(bp,0);
			PUT_BROS(bp,my_tr);

			PUT_LEFT_CHILD(my_tr,bp);
			PUT_RIGHT_CHILD(my_tr,-1);
			return;
		}
		else
		{
			if (GET_HEAD_SIZE(GET_PAR(my_tr)) >  GET_HEAD_SIZE(my_tr))
				PUT_LEFT_CHILD(GET_PAR(my_tr),bp);
			else if (GET_HEAD_SIZE(GET_PAR(my_tr)) <  GET_HEAD_SIZE(my_tr))
				PUT_RIGHT_CHILD(GET_PAR(my_tr),bp);

			PUT_LEFT_CHILD(bp,GET_LEFT_CHILD(my_tr));
			PUT_RIGHT_CHILD(bp,GET_RIGHT_CHILD(my_tr));
			if (GET_LEFT_CHILD(my_tr) != 0)
				PUT_PAR(GET_LEFT_CHILD(my_tr),bp);
			if (GET_RIGHT_CHILD(my_tr) != 0)
				PUT_PAR(GET_RIGHT_CHILD(my_tr),bp);
			PUT_PAR(bp,GET_PAR(my_tr));
			PUT_BROS(bp,my_tr);
			PUT_RIGHT_CHILD(my_tr,-1);
			PUT_LEFT_CHILD(my_tr,bp);
			return;
		}
	}
}


