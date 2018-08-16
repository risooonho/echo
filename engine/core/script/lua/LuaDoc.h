//Lua笔记-关于lua table的C API
//转载请注明来自yuliying的CSDN博客.
//Lua版本5.2

/*相关API:
	lua_createtable
	原型: void lua_createtable (lua_State *L, int narr, int nrec);
	描述: 创建一个新的table并将之放在栈顶.narr是该table数组部分的长度,nrec是该table hash部分的长度.
		  当我们确切的知道要放多少元素到table的时候,使用这个函数,lua可以预分配一些内存,提升性能.
		  如果不确定要存放多少元素可以使用 lua_newtable 函数来创建table.

	lua_newtable
	原型: void lua_newtable (lua_State *L);
	描述: 创建一个新的table并将之放在栈顶. 等同于lua_createtable(L, 0, 0).

	lua_getfield
	原型: void lua_getfield (lua_State *L, int index, const char *k);
	描述: 将t[k]元素push到栈顶. 其中t是index处的table.
		  这个函数可能触发index元方法.

	lua_setfield
	原型: void lua_setfield (lua_State *L, int index, const char *k);
	描述: 为table中的key赋值. t[k] = v . 其中t是index处的table , v为栈顶元素.
		  这个函数可能触发newindex元方法.
		  调用完成后弹出栈顶元素(value).

	lua_gettable
	原型: void lua_gettable (lua_State *L, int index);
	描述: 将t[k]元素push到栈顶. 其中t是index处的table,k为栈顶元素.
		  这个函数可能触发index元方法.
		  调用完成后弹出栈顶元素(key).

	lua_settable
	原型: void lua_settable (lua_State *L, int index);
	描述: 为table中的key赋值. t[k] = v . 其中t是index处的table , v为栈顶元素. k为-2处的元素.
		  这个函数可能触发newindex元方法.
		  调用完成后弹出栈顶两个元素(key , value)

	lua_rawget
	原型: void lua_rawget (lua_State *L, int index);
	描述: 与lua_gettable函数类似, 但是不会触发__index元方法.

	lua_rawset
	原型: void lua_rawset (lua_State *L, int index);
	描述: 与lua_settable函数类似, 但是不会触发newindex元方法.

	lua_rawgeti
	原型: void lua_rawgeti (lua_State *L, int index, int n);
	描述: 将t[n]元素push到栈顶.其中t是index处的table.
		  这个函数不会触发index元方法.

	lua_rawseti
	原型: void lua_rawseti (lua_State *L, int index, int n);
	描述: 为table中的key赋值. t[n] = v .其中t是index处的table , v为栈顶元素.
		  这个函数不会触发newindex元方法.
		  调用完成后弹出栈顶元素.

	lua_rawgetp
	原型: void lua_rawgetp (lua_State *L, int index, const void *p);
	描述: 将t[p]元素push到栈顶.其中t是index处的table. p是一个lightuserdata.
		  这个函数不会触发index元方法.

	lua_rawsetp
	原型: void lua_rawsetp (lua_State *L, int index, const void *p);
	描述: 为table中的key赋值. t[p] = v .其中t是index处的table , p是一个lightuserdata , v为栈顶元素.
		  这个函数不会触发newindex元方法.
		  调用完成后弹出栈顶元素.

	lua_getmetatable
	原型: int lua_getmetatable (lua_State *L, int index);
	描述: 将index处元素的元表push到栈顶. 如果该元素没有元表, 函数返回0 , 不改变栈.

	lua_setmetatable
	原型: void lua_setmetatable (lua_State *L, int index);
	描述: 将栈顶元素设置为index处元素的元表.
		  调用完成后弹出栈顶元素.

	lua_istable
	原型: int lua_istable (lua_State *L, int index);
	描述: 判断index处元素是否为一个table , 如果是返回1,否则返回0.

	lua_pushglobaltable
	原型: void lua_pushglobaltable (lua_State *L);
	描述: 将lua的全局表放在栈顶.

	luaL_newmetatable
	原型: int luaL_newmetatable (lua_State *L, const char *tname);
	描述: 如果注册表中已经有名为tname的key,则返回0.
		  否则创建一个新table作为userdata的元表. 这个元表存储在注册表中,并以tname为key. 返回1.
		  函数完成后将该元表置于栈顶.

	luaL_getmetatable
	原型: void luaL_getmetatable (lua_State *L, const char *tname);
	描述: 将注册表中以tname为key的元表push到栈顶.

	luaL_setmetatable
	原型: void luaL_setmetatable (lua_State *L, const char *tname);
	描述: 将栈顶元素存储到注册表中, 它的key为tname.

	luaL_getsubtable
	原型: int luaL_getsubtable (lua_State *L, int idx, const char *fname);
	描述: 将 t[fname] push到栈顶, 其中t是index处的table , 并且 t[fname] 也为一个table.
	如果 t[fname] 原本就存在,返回 true ,否则返回false,并且将 t[fname] 新建为一张空表.

	lua_getglobal
	原型: void lua_getglobal (lua_State *L, const char *name);
	描述: 将 t[name] 元素push到栈顶, 其中t为全局表.

	lua_setglobal
	原型: void lua_setglobal (lua_State *L, const char *name);
	描述: 为table中的key赋值. t[name] = v . 其中t为全局表. v为栈顶元素.
	调用完成后弹出栈顶元素(v).

	luaL_newlibtable
	原型: void luaL_newlibtable (lua_State *L, const luaL_Reg l[]);
	描述: 创建一张空表, lua预先分配足够的内存用来存储我们创建的函数库.
	稍后我们可以使用 luaL_setfuncs 函数注册我们的函数库.

	luaL_setfuncs
	原型: void luaL_setfuncs (lua_State *L, const luaL_Reg *l, int nup);
	描述: 将所有 luaL_Reg数组中的函数注册到栈顶的table中.
		  当upvalue个数不为0时,所创建的所有函数共享这些upvalue. -2到-(nup+1)的元素为要注册的upvalue.
		  (注意:这些upvalue是c中的upvalue,不是lua中的upvalue,可以在注册的c函数中通过 lua_upvalueindex(n)获取其值.)
		  调用完成后弹出栈顶的所有upvalue.

	luaL_newlib
	原型: void luaL_newlib (lua_State *L, const luaL_Reg *l);
	描述: 创建一个新的table , 并将luaL_Reg数组中的函数注册到其中.
		  它是一个宏 (luaL_newlibtable(L,l), luaL_setfuncs(L,l,0))

	lua_next
	原型: int lua_next (lua_State *L, int index);
	描述: 该函数用来遍历一个table.
		  从栈顶弹出一个key , 并且push一个 key-value对(栈顶key的下一个键值对) ,到栈顶.
		  如果table中没有更多的元素, 函数返回0.
		  遍历开始时栈顶为一个nil , 函数取出第一个键值对.

	通常遍历方法为:
	lua_pushnil(L);  // first key
	while (lua_next(L, t) != 0) {
		// uses 'key' (at index -2) and 'value' (at index -1)
		printf("%s - %s\n",
		lua_typename(L, lua_type(L, -2)),
		lua_typename(L, lua_type(L, -1)));
		// removes 'value'; keeps 'key' for next iteration
		lua_pop(L, 1);
	}
	注意: 在遍历table的时候 ,除非明确的知道key为字符串,不要对栈上的key使用 lua_tolstring 函数 ,
		  因为这样有可能改变key的类型 , 影响下一次 lua_next调用.

	lua_rawlen
	原型: size_t lua_rawlen (lua_State *L, int index);
	描述: 获取index处元素的长度.
		  对于字符串来说,返回字符串长度.
		  对于table来说,返回#操作符的长度. 不受元方法影响.
		  对于userdata来说,返回内存的大小.
		  其他元素返回0.

	lua_len
	原型: void lua_len (lua_State *L, int index);
	描述: 获取index处元素#操作符的结果 , 放置在栈顶.


	其他概念:
		1.伪索引:
			Lua栈的正常索引 从栈顶算,栈顶为-1,向栈低递减. 从栈低算,栈低为1,向栈顶递增.
			伪索引是一种索引,他不在栈的位置中,通过一个宏来定义伪索引的位置.
			伪索引被用来访问注册表,或者在lua_CFunction中访问upvalue.
		2.注册表:
			Lua的注册表是一个预定义的table, 可以提供给c api存储一切想要存储的值.
			注册表通过 LUA_REGISTRYINDEX 伪索引来访问.
			例如 lua_getfield 函数可以像下面这样使用来获取注册表中的一个以"hello"为key的值 :
			lua_getfield( L , LUA_REGISTRYINDEX , "hello");
		3. upvalue:
			在使用 lua_pushcfunction 或者 luaL_setfuncs 将一个lua_CFunction 注册到Lua环境中时,
			可以同时为这个函数设置一些upvalue .
			而后在这些lua_CFunction 中可以使用 lua_upvalueindex(n) 函数来获取对应位置的upvalue.
*/