#include <iostream>
#include <string>
#include <time.h>
#include <vector>
#include <list>
#include <ctime>
#include <iomanip>

/*******************************************************************************
*                             cache abstract                                   *
*******************************************************************************/

typedef unsigned long Addr;

typedef struct _CacheLine {
	unsigned long tag;
	unsigned long data;
}CacheLine;

int SDRAM_DDR[1024 * 1024 * 8];

#define MemoryAccess(addr) (SDRAM_DDR[addr])

int log2(int x)
{
	int res = 0;
	while (x) {
		x = x >> 1;
		res++;
	}
	return res;
}

class Cache {
public:
	Cache() = default;
	Cache(int _size, int _assoc) : size(_size), assoc(_assoc) { 
		cache = new Set[(int)(size / assoc)];
		indexBits = log2((int)(size / assoc));
	}
	~Cache() { delete[]cache; }
	void cacheInit() {
		CacheLine tmp_cache_line = { -1, 0 };
		for (int i = 0; i < (int)(size / assoc); ++i) {
			cache[i].insert(cache[i].end(), assoc, tmp_cache_line);
		}
	}
	typedef std::list<CacheLine> Set;
	Addr extractSet(Addr addr) { return (int)((addr >> offsetBits) & indexBits); }
	Addr extractTag(Addr addr) { return (Addr)((addr >> offsetBits) >> indexBits); }
	virtual Set::iterator FindVictim(int set) = 0;
	virtual bool access(Addr addr, unsigned long *res_data) {
		bool result = false;
		Addr tag = extractTag(addr);
		int set = extractSet(addr);
		for(auto i = cache[set].begin(); i != cache[set].end(); ++i)
			if (tag == i->tag) {
				*res_data = i->data;
				result = true;
				break;
			}
		if (result == false) {
			Set::iterator Victim = FindVictim(set);
			int data = MemoryAccess(addr);
			Victim->tag = tag;
			Victim->data = data;
		}
		return result;
	}
	virtual bool writeMem(Addr addr, unsigned long data) {
		bool result = false;
		Addr tag = extractTag(addr);
		int set = extractSet(addr);
		for (auto i = cache[set].begin(); i != cache[set].end(); ++i)
			if (tag == i->tag) {
				i->data = data;
				SDRAM_DDR[addr] = data;
				result = true;
				break;
			}
		if (result == false) {
			Set::iterator Victim = FindVictim(set);
			Victim->tag = tag;
			Victim->data = data;
		}
		return result;
	}
	virtual int getSize() const { return size; }
	virtual int getAssoc() const { return assoc; }
	int getIndexBits() { return indexBits; }
protected:
	int size;
	int assoc;
	Set *cache = NULL;
	int indexBits;
	static const int cacheLineSize = 4;
	static const int offsetBits = 2;
};

class LRUCache : public Cache {
public:
	LRUCache() = default;
	LRUCache(int _size, int _assoc) : Cache(_size, _assoc) { cacheInit(); }
	int getSize() const override { return size; }
	int getAssoc() const override { return assoc; }
	Set::iterator FindVictim(int set) override {
		Set::iterator i;
		i = cache[set].end();
		--i;
		return i;
	}
	bool access(Addr addr, unsigned long *res_data) override {
		bool result = false;
		Addr tag = extractTag(addr);
		int set = extractSet(addr);
		for (auto i = cache[set].begin(); i != cache[set].end(); ++i) {
			if (tag == i->tag) {	// tag对上了，且距离小于关联度（即还留在cache中）
				*res_data = i->data;
				result = true;
				cache[set].push_front(*i);
				cache[set].erase(i);
				break;
			}
		}
		if (result == false) {
			Set::iterator Victim = FindVictim(set);
			int data = MemoryAccess(addr);
			Victim->tag = tag;
			Victim->data = data;
			cache[set].push_front(*Victim);
			cache[set].erase(Victim);
		}
		return result;
	}
	bool writeMem(Addr addr, unsigned long data) override {
		bool result = false;
		Addr tag = extractTag(addr);
		int set = extractSet(addr);
		for (auto i = cache[set].begin(); i != cache[set].end(); ++i)
			if (tag == i->tag) {
				i->data = data;
				result = true;
				break;
			}
		if (result == false) {
			Set::iterator Victim = FindVictim(set);
			Victim->tag = tag;
			Victim->data = data;
			cache[set].push_front(*Victim);
			cache[set].erase(Victim);
		}
		SDRAM_DDR[addr] = data;
		return result;
	}
};

class RandomCache : public Cache {
public:
	RandomCache() = default;
	RandomCache(int _size, int _assoc) : Cache(_size, _assoc) { cacheInit(); }
	int getSize() const override { return size; }
	int getAssoc() const override { return assoc; }
	Set::iterator FindVictim(int set) override {
		srand((int)time(0));		// 产生随机种子
		int index_num = rand() % assoc;
		Set::iterator i = cache[set].begin();
		while (index_num--)
			++i;
		return i;
	}
	bool access(Addr addr, unsigned long *res_data) override {
		bool result = false;
		Addr tag = extractTag(addr);
		int set = extractSet(addr);
		int i_cnt = 0;
		for (auto i = cache[set].begin(); i != cache[set].end(); ++i) {
			if (tag == i->tag && i_cnt < assoc) {	// tag对上了，且距离小于关联度（即还留在cache中）
				*res_data = i->data;
				result = true;
				cache[set].push_front(*i);
				cache[set].erase(i);
				break;
			}
			i_cnt++;
		}
		if (result == false) {
			Set::iterator Victim = FindVictim(set);
			int data = MemoryAccess(addr);
			Victim->tag = tag;
			Victim->data = data;
			cache[set].push_front(*Victim);
			cache[set].erase(Victim);
		}
		return result;
	}
	bool writeMem(Addr addr, unsigned long data) override {
		bool result = false;
		Addr tag = extractTag(addr);
		int set = extractSet(addr);
		for (auto i = cache[set].begin(); i != cache[set].end(); ++i)
			if (tag == i->tag) {
				i->data = data;
				result = true;
				break;
			}
		if (result == false) {
			Set::iterator Victim = FindVictim(set);
			Addr addr2DDR = (Victim->tag << (indexBits + 2)) | (set << 2);
			SDRAM_DDR[addr2DDR] = data;
			Victim->tag = tag;
			Victim->data = data;
			cache[set].push_front(*Victim);
			cache[set].erase(Victim);

		}
		SDRAM_DDR[addr] = data;
		return result;
	}
};

void CacheAccess(Cache *cache, Addr addr) {
	unsigned long data;
	bool res;
	res = cache->access(addr, &data);
	std::cout << "Read\t0x" << std::hex << std::setw(5) << std::setfill('0') << addr << ":\t" \
		<< std::setw(8) << data << "\t" << (res == true ? "Hit" : "Miss") << std::endl;
}

void CacheWrite(Cache *cache, Addr addr, unsigned long data) {
	bool res;
	res = cache->writeMem(addr, data);
	std::cout << "Write\t0x" << std::hex << std::setw(5) << std::setfill('0') << addr << ":\t" \
		<< std::setw(8) << data << "\t" << (res == true ? "Hit" : "Miss") << std::endl;
}

int main()
{
	std::cout << std::endl << "-----------------------------" << std::endl << std::endl;

	LRUCache *cache = new LRUCache(16 * 1024, 4);
	unsigned long data;
	bool res;
	CacheAccess(cache, 0x00000000);
	CacheAccess(cache, 0x00010000);
	CacheAccess(cache, 0x00020000);
	CacheAccess(cache, 0x00030000);
	CacheAccess(cache, 0x00000000);
	CacheAccess(cache, 0x00000000);
	CacheAccess(cache, 0x00010000);
	CacheAccess(cache, 0x00010000);
	CacheAccess(cache, 0x00020000);
	CacheAccess(cache, 0x00020000);
	CacheAccess(cache, 0x00030000);
	CacheAccess(cache, 0x00040000);
	CacheAccess(cache, 0x00040000);
	CacheAccess(cache, 0x00000000);
	CacheWrite( cache, 0x00050000, 0x0422);
	CacheAccess(cache, 0x00050000);
	CacheAccess(cache, 0x00030000);
	CacheAccess(cache, 0x00020000);
	CacheAccess(cache, 0x00020000);
	std::cout << std::endl << "-----------------------------" << std::endl << std::endl;

	Cache *pc = cache;
	CacheAccess(pc, 0x00020010);
	CacheWrite(pc, 0x00020010, 0x1234);
	CacheAccess(pc, 0x00020020);
	CacheAccess(pc, 0x00020010);

	delete cache;


	system("pause");
	return 0;
}
