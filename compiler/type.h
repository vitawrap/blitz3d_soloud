
#ifndef TYPE_H
#define TYPE_H

#include "decl.h"

struct FuncType;
struct ArrayType;
struct StructType;
struct ConstType;
struct VectorType;

struct Type{
	virtual ~Type(){}

	virtual bool intType(){ return 0;}
	virtual bool floatType(){ return 0; }
	virtual bool stringType(){ return 0; }

	//casts to inherited types
	virtual FuncType *funcType(){ return 0; }
	virtual ArrayType *arrayType(){ return 0; }
	virtual StructType *structType(){ return 0; }
	virtual ConstType *constType(){ return 0; }
	virtual VectorType *vectorType(){ return 0; }

	//operators
	virtual bool canCastTo( Type *t ){ return this==t; }

	//built in types
	static Type *void_type,*int_type,*float_type,*string_type,*null_type;
};

struct FuncType : public Type{
	Type *returnType;
	DeclSeq *params;
	bool userlib,cfunc,vfunc;
	FuncType( Type *t,DeclSeq *p,bool ulib,bool cfn,bool vfn = false ):returnType(t),params(p),userlib(ulib),cfunc(cfn),vfunc(vfn){}
	~FuncType(){ delete params; }
	FuncType *funcType(){ return this; }
};

struct ArrayType : public Type{
	Type *elementType;int dims;
	ArrayType( Type *t,int n ):elementType(t),dims(n){}
	ArrayType *arrayType(){ return this; }
};

struct StructType : public Type{
	string ident;
	StructType* base;
	DeclSeq *fields;
	DeclSeq *virtuals;
	StructType( const string &i ):ident(i),fields(0),virtuals(0),base(0){}
	StructType( const string &i,DeclSeq *f ):ident(i),fields( f ),virtuals(0),base(0){}
	StructType( const string &i,DeclSeq *f,StructType* b ):ident(i),fields( f ),virtuals(0),base(b){}
	~StructType() { delete fields; delete virtuals; }
	StructType *structType(){ return this; }
	virtual bool canCastTo( Type *t );
	int countFields() const;
	int countVirtuals() const;
	Decl* findField(const string& ident);
	Decl* findVirtual(const string& ident);
	void getStructTypeChain(list<StructType*>&);
	bool isTypeInChain(Type* type) const;
};

struct ConstType : public Type{
	Type *valueType;
	int intValue;
	float floatValue;
	string stringValue;
	ConstType( int n ):intValue(n),valueType(Type::int_type){}
	ConstType( float n ):floatValue(n),valueType(Type::float_type){}
	ConstType( const string &n ):stringValue(n),valueType(Type::string_type){}
	ConstType *constType(){ return this; }
};

struct VectorType : public Type{
	string label;
	Type *elementType;
	vector<int> sizes;
	VectorType( const string &l,Type *t,const vector<int> &szs ):label(l),elementType(t),sizes(szs){}
	VectorType *vectorType(){ return this; }
	virtual bool canCastTo( Type *t );
};

#endif
