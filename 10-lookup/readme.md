# 高效IP路由查找实验

- - -
## 实验内容

#### 实现最基本的前缀树查找

#### 调研并实现某种IP前缀查找方案

#### 基于forwarding-table.txt数据集(Subnet, Prefix Length, Port)

- 对比基本前缀树和所实现IP前缀查找的性能
  - 内存开销、平均单次查找时间

#### 实现性能评估

##### 内存开销

- 在C语言中（使用其他编程语言类似），计算每次申请内存时的大小，内存开销为最终申请的所有内存大小之和

##### 平均查找时间

- 在保证查找结果正确的前提下，将forwarding-table.txt中的IP地址作为输入，计算总的查找时间，再除以条目数

## 实验步骤

### 1. 基本前缀树 (naive_trie.c)

- 前缀树数据结构

  ```c
  typedef struct trie
  {
      struct trie *child[CHILD_NUM];
      u8 match;	// determine if it is a last node of an IP 
  } Trie_node_t;
  ```

- **关键函数**
  - **生成新结点 **`new_trie_node`   
  - **插入新结点 **`insert_trie_node`  
  - **查找结点** `lookup_trie_node`  

### 2. 多bit前缀树 (multi_trie.c)

- 多bit前缀树通过增加每次匹配的bit树，来减小内存开销。原理如下图所示：

  ![](pic/multi-bit.PNG)

- 本实验实现bit = 2，3，4，… 的情况；修改bit的值通过修改`trie.h`中的宏定义`BIT_STEP`得到。

- 多bit前缀树结点**数据结构**

  ```c
  typedef struct multi_trie
  {
      struct multi_trie *child[1 << BIT_STEP];
      u8 match;
  } Multi_trie_t;
  ```

- 在单bit前缀树的基础上修改**插入**和**查找**函数，将每次查找结点的键值改为如下形式，可以实现每个节点匹配多bit IP。

  ```c
  bits = (ip >> (32 - BIT_STEP - i)) & (0xF >> (4 - BIT_STEP));
  ```

- **插入新结点**

  ```c
  void insert_multi_trie_node(Multi_trie_t *trie, u32 ip, u32 mask_len)
  {
      Multi_trie_t *p = trie;
      u32 bits = 0;
      for (int i = 0; i < mask_len; i += BIT_STEP) {
          bits = (ip >> (32 - BIT_STEP - i)) & (0xF >> (4 - BIT_STEP));
          if (!p->child[bits])
              p->child[bits] = new_multi_trie_node(0);
          p = p->child[bits];
      }
      if (p->match) {
          printf("[insert trie node] node already exist.\n");
          return;
      }
      p->match = 1;
  }
  ```

- **查找结点**

  ```c
  int lookup_multi_trie_node(Multi_trie_t *trie, u32 ip)
  {
      Multi_trie_t *p = trie;
      u32 bits = 0;
      int found = 0;
      for (int i = 0; p != NULL; i += BIT_STEP) {
          found = p->match;
          bits = (ip >> (32 - BIT_STEP - i)) & (0xF >> (4 - BIT_STEP));
          p = p->child[bits];
      }
      return found;
  }
  ```

### 3. Poptrie: 快速可扩展压缩前缀树 (poptrie.c)

> **基本原理：**
>
> 将多路trie中的后代阵列改变为按位阵列。向量base0和base1用作后代数组。
> 向量指示对应的后代内部节点，如果没有后代内部节点，则搜索该树的该级别的叶节点。
> 树中搜索的下一个节点：由于当前k比特地址组块的值n对应于向量中的第n个比特，因此最小显着n + 1中的1的个数向量的位可以用作当前内部节点的后代数组中的下一个节点的索引。
> 
>**查找算法：**
> 
>查找算法根据指定的IP地址逐步降低树。在深度d处，地址的第d个块用作内部节点中的矢量的索引。令第d个块的值为n，然后执行d深度处的查找：如果对应位为1，则继续到下一个深度查找。
> 后代数组中下一个内部节点的索引是通过向base1添加向量中最不重要的n + 1位的1的数量减1来计算的。如果相应的位为零，则查找算法结束返回叶子查找。
>叶子阵列中叶子节点的索引是通过将向量的最小值n + 1位减去1的0的数量加到base0来计算的。
> 通过计算位串中的1和0的数量，计算后代节点和叶节点的间接索引，通过指令popcnt实现。当popcnt CPU算法1中显示了k = 6的查找算法。该算法将poptrie结构t和IP地址密钥作为输入，并返回最长匹配的叶子在t中，有内部节点阵列算法1查找（t =（N，L），key）;树t中地址键的查找过程（当k = 6时）。函数extract（key，offset，len）从地址键中提取长度为len的位，从offset 开始。 N和L分别表示内部节点和叶子的数组。 v表示位的移位指令。
> 
> **算法一：**
> 
> 1: index = 0;
>2: vector = t.N[index].vector;
> 3: offset = 0;
> 4: v = extract(key, offset, 6);
> 5: while (vector & (1ULL << v)) do
> 6: base = t.N[index].base1;
> 7: bc = popcnt(vector & ((2ULL << v) - 1));
> 8: index = base + bc - 1;
> 9: vector = t.N[index].vector;
> 10: offset += 6;
> 11: v = extract(key, offset, 6);
> 12: end while
> 13: base = t.N[index].base0;
> 14: bc = popcnt((~t.N[index].vector) & ((2ULL << v) - 1));
> 15: return t.L[base + bc - 1];

- poptrie前缀树结点**数据结构**

  ```c
  typedef struct poptrie
  {
  	u16 vector;
  	u16 leafvec;
  	u32 base0;
  	u32 base1;
  } Poptrie_node_t;
  ```
  
- **压缩前缀树**

  ```c
  void compress_poptrie(Multi_trie_t *trie, Poptrie_node_t *pop_trie)
  {
      u32 bits;
      pop_trie->base0 = allocated_leaf;
      pop_trie->base1 = allocated_node;
      for (int i = 0; i < MAX_KEY; i ++) {
          if (!trie->child[i]) continue;
          if (is_leaf(trie->child[i])) {
              pop_trie->leafvec |= 1 << i;
              L[allocated_leaf] = 1;
              allocated_leaf ++;
          }
          else {
              pop_trie->vector |= 1 << i;
              allocated_node ++;
          }
      }
      for (int i = 0; i < MAX_KEY; i ++) {
          if (trie->child[i] && !is_leaf(trie->child[i])){
              bits = pop_trie->base1 + popcnt(pop_trie->vector & ((2ULL << i) - 1)) - 1;
              compress_poptrie(trie->child[i], &N[bits]);
          }
      }
  }
  ```

- **生成poptrie**

  - 此过程包含不断压缩前缀树的递归过程

  ```c
  void build_poptrie(Poptrie_node_t *poptrie)
  {
      count_node_leaf(poptrie);
      
      N = (Poptrie_t *)malloc(sizeof(Poptrie_t) * noden);
      L = (Poptrie_leaf_t *)malloc(sizeof(Poptrie_leaf_t) * leafn);
  
      printf(" [Size of Poptrie] %lu MB\n", (sizeof(Poptrie_t)*noden+sizeof(Poptrie_t)*leafn)/1048576);
      bzero(L, sizeof(L));
      bzero(N, sizeof(N));
      Poptrie = N;
      compress_poptrie(poptrie, Poptrie);
  }
  ```

- **查找结点**

  ```c
  u32 lookup_poptrie_node(u32 ip)
  {
      // Poptrie_node_t *p = poptrie;
      u16 vec = N[0].vec;
      u16 v = (ip >> (32 - BIT_STEP)) & 0xF;
      u32 offset = 0, bits = 0, num, res = 0;
      while (vec & (1ULL << v)) {
          if (N[bits].leafvec & (1ULL << v)) {
              res = L[N[bits].base0 + popcnt(N[bits].leafvec & ((2ULL << v) - 1)) - 1];
          }
          bits = N[bits].base1 + popcnt(vec & ((2 << v) - 1)) - 1;
          vec = N[bits].vec;
          offset += BIT_STEP;
          v = (ip >> (32 - BIT_STEP - offset)) & 0xF;
  
      }
      if (N[bits].leafvec & (1ULL << v)) {
          res = L[N[bits].base0 + popcnt(N[bits].leafvec & ((2ULL << v) - 1)) - 1];
      }
      return res;
  }
  ```

### 4. 测试查找结果 (main.c)

- **解析IP地址**  `parse_ip_port`（以基础前缀树为例，下同）

  ```c
  u32 parse_ip_port(char *str)
  {
  	int i1, i2, i3, i4;
      sscanf(str, "%d.%d.%d.%d", &i1, &i2, &i3, &i4);
  	return (i1 << 24) | (i2 << 16) | (i3 << 8) | i4;
  }
  ```

- **构建前缀树**：将从文件中解析的Ip地址插入到前缀树中，并将IP地址记录到`ip_buf`数组中。

  ```c
  while (!feof(file))
  {
      if (fscanf(file, "%s %u %d", ip_str, &mask, &port) < 0)
          break;
      ip = parse_ip_port(ip_str);
      insert_trie_node(trie, ip, mask);
      ip_buf[cnt++] = ip;
  }
  ```

- **查找IP**， 将结果写入`result.txt`中。

  ```c
  gettimeofday(&t1, NULL);
  for (int i = 0; i < cnt; i ++) {
      found = lookup_trie_node(trie, ip_buf[i]);
      fprintf(res, IP_FMT"\t%u\n", HOST_IP_FMT_STR(ip_buf[i]), found);
  }
  gettimeofday(&t2, NULL);
  ```

- **计算内存开销和平均时间开销**

  ```c
  // compute memory overhead
  printf(" [Space Overhead] %lu MB\n", sizeof(Trie_node_t)*node_num/1048576);
  
  // compute average search time
  double total = (t2.tv_sec-t1.tv_sec)*1000000+t2.tv_usec-t1.tv_usec;
  printf(" [Average Time Overhead] %lf us\n\n", total/cnt);
  ```

- **验证查找结果**

  - 分别对比naive trie和multi-bit trie、naive trie和poptrie查找结果的`txt`文件，如果不同，则报错。
  
  ```c
      FILE *diff1 = popen("diff trie-result.txt multi-result.txt", "r");
      char ch1 = fgetc(diff1);
      FILE *diff2 = popen("diff trie-result.txt poptrie-result.txt", "r");
      char ch2 = fgetc(diff2);
      if (ch1 == EOF && ch2 == EOF)
          printf(" [Poptrie] Correct\n");
      else
          printf("%c, %c\n", ch1, ch2);
      pclose(diff1);
      pclose(diff2);
  ```

## 实验结果

- 三种前缀树运行结果如下所示：

  ![](./pic/res.PNG)


- **结果对比**

  多次运行，取平均值如下：

  |       压缩树种类        | 空间开销(MB) | 时间开销(ns) |
  | :---------------------: | :----------: | :----------: |
  |       naive trie        |      37      |     165      |
  | multi-bit trie (2 bits) |      24      |     129      |
  | multi-bit trie (3 bits) |      20      |     968      |
  | multi-bit trie (4 bits) |      18      |     535      |
  |    poptrie (4 bits)     |      9       |     151      |

- **结果分析**

  由上表可知，从上到下空间开销和时间开销依次减小；

  4比特前缀树的空间开销减少了51.4%，时间开销减少了67.6%；

  poptrie的空间开销减少了75.7%，时间开销减少了90.9%。性能提升效果十分显著。

## 参考文献

1. [Efficient Construction Of Variable-Stride Multibit Tries For IP Lookup， Sartaj Sahni & Kun Suk Kim](<https://raminaji.wordpress.com/>)
2. *A Compressed Trie with Population Count for Fast and Scalable Software IP Routing Table Lookup*, Hirochika Asai, Yasuhiro Ohara.
3. [Github/pixos/poptrie,  Hirochika Asai](<https://github.com/pixos/poptrie>)