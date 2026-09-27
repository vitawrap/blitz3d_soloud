
#include "std.h"
#include "nodes.h"

static char const* VIRT_LSEP = "_T_";

//////////////////////////////
// Sequence of declarations //
//////////////////////////////
void DeclSeqNode::proto( DeclSeq *d,Environ *e ){
	for( int k=0;k<decls.size();++k ){
		try{ decls[k]->proto( d,e ); }
		catch( Ex &x ){ 
			if( x.pos<0 ) x.pos=decls[k]->pos;
			if(!x.file.size() ) x.file=decls[k]->file;
			throw; 
		}
	}
}

void DeclSeqNode::semant( Environ *e ){
	for( int k=0;k<decls.size();++k ){
		try{ decls[k]->semant( e ); }
		catch( Ex &x ){ 
			if( x.pos<0 ) x.pos=decls[k]->pos;
			if(!x.file.size() ) x.file=decls[k]->file;
			throw; 
		}
	}
}

void DeclSeqNode::buildvirt( Environ *e ){
	for( int k=0;k<decls.size();++k ){
		try{ decls[k]->buildvirt( e ); }
		catch( Ex &x ){ 
			if( x.pos<0 ) x.pos=decls[k]->pos;
			if(!x.file.size() ) x.file=decls[k]->file;
			throw; 
		}
	}
}

void DeclSeqNode::translate( Codegen *g ){
	for( int k=0;k<decls.size();++k ){
		try{ decls[k]->translate( g ); }
		catch( Ex &x ){
			if( x.pos<0 ) x.pos=decls[k]->pos;
			if(!x.file.size() ) x.file=decls[k]->file;
			throw; 
		}
	}
}

void DeclSeqNode::transdata( Codegen *g ){
	for( int k=0;k<decls.size();++k ){
		try{ decls[k]->transdata( g ); }
		catch( Ex &x ){ 
			if( x.pos<0 ) x.pos=decls[k]->pos;
			if(!x.file.size() ) x.file=decls[k]->file;
			throw; 
		}
	}
}

////////////////////////////
// Simple var declaration //
////////////////////////////
void VarDeclNode::proto( DeclSeq *d,Environ *e ){

	Type *ty=tagType( tag,e );
	if( !ty ) ty=Type::int_type;
	ConstType *defType=0;

	if( expr ){
		expr=expr->semant( e );
		expr=expr->castTo( ty,e );
		if( constant || (kind&DECL_PARAM) ){
			ConstNode *c=expr->constNode();
			if( !c ) ex( "Expression must be constant" );
			if( ty==Type::int_type ) ty=d_new ConstType( c->intValue() );
			else if( ty==Type::float_type ) ty=d_new ConstType( c->floatValue() );
			else ty=d_new ConstType( c->stringValue() );
			e->types.push_back( ty );
			delete expr;expr=0;
		}
		if( kind&DECL_PARAM ){
			defType=ty->constType();ty=defType->valueType;
		}
	}else if( constant ) ex( "Constants must be initialized" );

	Decl *decl=d->insertDecl( ident,ty,kind,defType );
	if( !decl ) ex( "Duplicate variable name" );
	if( expr ) sem_var=d_new DeclVarNode( decl );
}

void VarDeclNode::semant( Environ *e ){
}

void VarDeclNode::translate( Codegen *g ){
	if( kind & DECL_GLOBAL ){
		g->align_data( 4 );
		g->i_data( 0,"_v"+ident );
	}
	if( expr ) g->code( sem_var->store( g,expr->translate( g ) ) );
}

//////////////////////////
// Function Declaration //
//////////////////////////
void FuncDeclNode::proto( DeclSeq *d,Environ *e ){
	Type *t=tagType( tag,e );if( !t ) t=Type::int_type;
	a_ptr<DeclSeq> decls( d_new DeclSeq() );
	params->proto( decls,e );
	sem_type=d_new FuncType( t,decls.release(),false,false,virtual_first_arg );
	// register a different name for the compiler if virtual.
	if( !d->insertDecl( virtual_first_arg?
		ident+VIRT_LSEP+sem_type->params->decls[0]->type->structType()->ident : ident, sem_type, DECL_FUNC)) {
		delete sem_type; ex( "duplicate identifier" );
	}
	e->types.push_back( sem_type );

	if (virtual_first_arg) {
		// also if we're a virtual, claim the symbol name at compile time
		Decl* func = d->findDecl(ident);
		if (func && func->type->funcType() && !func->type->funcType()->vfunc)
			ex("a method may not use the same name as a function");

		d->insertDecl(ident, sem_type, DECL_FUNC);

		// register virtual method in struct arg
		Decl* virt_decl = sem_type->params->decls[0];
		if (virt_decl && virt_decl->type->structType()) {
			StructType* sem_this = virt_decl->type->structType();
			if (!sem_this->virtuals)
				sem_this->virtuals = new DeclSeq;
			if (!sem_this->virtuals->insertDecl(ident, sem_type, DECL_FUNC))
				ex("duplicate type method");
		}
	}
}

void FuncDeclNode::semant( Environ *e ){

	sem_env=d_new Environ( genLabel(),sem_type->returnType,1,e );
	DeclSeq *decls=sem_env->decls;

	int k;
	for( k=0;k<sem_type->params->size();++k ){
		Decl *d=sem_type->params->decls[k];
		if( !decls->insertDecl( d->name,d->type,d->kind ) ) ex( "duplicate identifier" );
	}

	stmts->semant( sem_env );
}

void FuncDeclNode::translate( Codegen *g ){

	//var offsets
	int size=enumVars( sem_env );

	//enter function
	if (virtual_first_arg) {
		Decl* virt_decl = sem_type->params->decls[0];
		if (virt_decl && virt_decl->type->structType()) {
			ident += VIRT_LSEP + virt_decl->type->structType()->ident;
		}
	}
	g->enter( "_f"+ident,size );

	//initialize locals
	TNode *t=createVars( sem_env );
	if( t ) g->code( t );
	if( g->debug ){
		string t=genLabel();
		g->s_data( ident,t );
		g->code( call( "__bbDebugEnter",local(0),iconst((int)sem_env),global(t) ) );
	}

	//translate statements
	stmts->translate( g );

	for( int k=0;k<sem_env->labels.size();++k ){
		if( sem_env->labels[k]->def<0 )	ex( "Undefined label",sem_env->labels[k]->ref );
	}

	//leave the function
	g->label( sem_env->funcLabel+"_leave" );
	t=deleteVars( sem_env );
	if( g->debug ) t=d_new TNode( IR_SEQ,call( "__bbDebugLeave" ),t );
	g->leave( t,sem_type->params->size()*4 );
}

//////////////////////
// Type Declaration //
//////////////////////
static map<string, StructDeclNode*> _knownDecls;

void StructDeclNode::registerDeclNode(StructDeclNode *n) {
	if (_knownDecls.find(n->ident) == _knownDecls.end()) {
		_knownDecls[n->ident] = n;
	}
}

void StructDeclNode::resetDeclNodes() {
	_knownDecls.clear();
}

StructDeclNode* StructDeclNode::findDeclNode(string const& ident) {
	auto itr = _knownDecls.find(ident);
	return itr == _knownDecls.end() ? 0 : itr->second;
}

void StructDeclNode::getDeclTypeChainNodes(string const& ident, list<StructDeclNode*>& nodes) {
	for (auto* walk = findDeclNode(ident); walk; walk = findDeclNode(walk->base))
		nodes.push_front(walk);
}

void StructDeclNode::proto( DeclSeq *d,Environ *e ){
	StructType* bstruct = 0;
	if (base != "") {
		auto* btype = d->findDecl(base);
		if (!btype) ex("Base type does not exist");
		if (btype->kind != DECL_STRUCT) ex("Base type must be a NewType");
		bstruct = btype->type->structType();
	}
	sem_type=d_new StructType( ident,d_new DeclSeq(),bstruct );
	if( !d->insertDecl( ident,sem_type,DECL_STRUCT ) ){
		delete sem_type;ex( "Duplicate identifier" );
	}
	e->types.push_back( sem_type );
	//registerDeclNode(this);
}

void StructDeclNode::semant( Environ *e ){
	// proto is called for this type first to get sem_type.
	int base_offset = 0;
	for (auto* walk = sem_type->base; walk; walk = walk->base) {
		base_offset += walk->fields->size() * 4;
	}

	// apply semant on all fields
	//list<StructDeclNode*> nodes;
	//getDeclTypeChainNodes(ident, nodes);
	//for (auto* node : nodes)
	//	node->fields->proto( sem_type->fields,e );
	fields->proto( sem_type->fields,e );

	for( int k=0;k<sem_type->fields->size();++k ) {
		sem_type->fields->decls[k]->offset = base_offset + (k*4); // every datatype in bb can be stored in 4 bytes
	}
}

static const string INHERIT_TAG = "INHERITED";

// this runs after all structs are known and all funcs have run proto
// structs should also be linked in extend order
void StructDeclNode::buildvirt( Environ *e ){
	
	StructType* sem_base = sem_type->base;

	if (sem_base && sem_base->virtuals) {
		// register all inherited virtuals alongside new or overridden ones
		for (int k=0;k<sem_base->virtuals->size();++k) {
			Decl* base_virt = sem_base->virtuals->decls[k];
			string test_ident = strstr(base_virt->name.c_str(), INHERIT_TAG.c_str())?
				base_virt->name.substr(0,base_virt->name.length() - INHERIT_TAG.length()) : base_virt->name;
			if (!sem_type->virtuals)
				sem_type->virtuals = new DeclSeq();
			Decl* proto_virt = sem_type->virtuals->findDecl(test_ident);
			if (!proto_virt)
				sem_type->virtuals->insertDecl(test_ident+INHERIT_TAG, base_virt->type, base_virt->kind);
			else {
				// if the new/override decl exists, reinsert it at the end to keep order of offsets...
				sem_type->virtuals->removeDecl(proto_virt);
				sem_type->virtuals->insertDecl(proto_virt);
			}
		}

		// and calculate vtable offsets
		int base_virts = sem_base->virtuals->size() * 4; // only increment when not inherited or overridden
		for (int k=0; sem_type->virtuals && k < sem_type->virtuals->size();++k) {
			Decl* virt = sem_type->virtuals->decls[k];
			string test_ident = strstr(virt->name.c_str(), INHERIT_TAG.c_str())?
				virt->name.substr(0,virt->name.length() - INHERIT_TAG.length()) : virt->name;
			// point to known offset if override or inherited, add to list if this is a new virtual
			Decl* over_virt = sem_base->virtuals->findDecl(test_ident);
			if (!over_virt) // parent may have no impl
				over_virt = sem_base->virtuals->findDecl(test_ident+INHERIT_TAG);
			if (over_virt) {
				virt->offset= over_virt->offset; continue;
			}
			virt->offset= base_virts + (k * 4);
		}
	} else {
		// no parent class so we're free to just generate a root table
		for (int k=0; sem_type->virtuals && k < sem_type->virtuals->size();++k) {
			sem_type->virtuals->decls[k]->offset= k * 4;
		}
	}
}

void StructDeclNode::translate( Codegen *g ){
	//translate fields
	//list<StructDeclNode*> nodes;
	//getDeclTypeChainNodes(ident, nodes);
	//for (auto* node : nodes)
	//	node->fields->translate( g );
	fields->translate( g );

	int k;
	list<StructType*> sem_types;

	//vtable (sem_type is completed in translate)
	string vtlabel = "_v_t" + ident;
	g->align_data( 4 );
	g->i_data(sem_type->countVirtuals(), vtlabel); // number of virtual members (new/overriden/inherited)
	for ( k=0;sem_type->virtuals && k < sem_type->virtuals->size();++k ){
		StructType* vftype = sem_type;
		Decl* virt = vftype->virtuals->decls[k];
		while (strstr(virt->name.c_str(), INHERIT_TAG.c_str())) {
			vftype = vftype->base;
			virt = vftype->virtuals->decls[k];
		}
		g->p_data("_f"+virt->name+VIRT_LSEP+vftype->ident);
	}

	//type ID
	g->align_data( 4 );
	g->i_data( 5,"_t"+ident ); // BBType header
	g->p_data( "_v_t"+ident ); // BBObjType vtable ptr

	//used and free lists for type
	for( k=0;k<2;++k ){
		string lab=genLabel();
		g->i_data( 0,lab );	//fields
		g->p_data( lab );	//next
		g->p_data( lab );	//prev
		g->i_data( 0 );		//type
		g->i_data( -1 );	//ref_cnt
	}

	//number of fields
	g->i_data( sem_type->countFields() );

	//type of each field
	sem_type->getStructTypeChain(sem_types); // includes itself
	for ( auto* chain_type : sem_types ) {
		for( k=0;k<chain_type->fields->size();++k ){
			Decl *field=chain_type->fields->decls[k];
			Type *type=field->type;
			string t;
			if( type==Type::int_type ) t="__bbIntType";
			else if( type==Type::float_type ) t="__bbFltType";
			else if( type==Type::string_type ) t="__bbStrType";
			else if( StructType *s=type->structType() ) t="_t"+s->ident;
			else if( VectorType *v=type->vectorType() ) t=v->label;
			g->p_data( t );
		}
	}

}

//////////////////////
// Data declaration //
//////////////////////
void DataDeclNode::proto( DeclSeq *d,Environ *e ){
	expr=expr->semant( e );
	ConstNode *c=expr->constNode();
	if( !c ) ex( "Data expression must be constant" );
	if( expr->sem_type==Type::string_type ) str_label=genLabel();
}

void DataDeclNode::semant( Environ *e ){
}

void DataDeclNode::translate( Codegen *g ){
	if( expr->sem_type!=Type::string_type ) return;
	ConstNode *c=expr->constNode();
	g->s_data( c->stringValue(),str_label );
}

void DataDeclNode::transdata( Codegen *g ){
	ConstNode *c=expr->constNode();
	if( expr->sem_type==Type::int_type ){
		g->i_data( 1 );g->i_data( c->intValue() );
	}else if( expr->sem_type==Type::float_type ){
		float n=c->floatValue();
		g->i_data( 2 );g->i_data( *(int*)&n );
	}else{
		g->i_data( 4 );g->p_data( str_label );
	}
}

////////////////////////
// Vector declaration //
////////////////////////
void VectorDeclNode::proto( DeclSeq *d,Environ *env ){

	Type *ty=tagType( tag,env );if( !ty ) ty=Type::int_type;

	vector<int> sizes;
	for( int k=0;k<exprs->size();++k ){
		ExprNode *e=exprs->exprs[k]=exprs->exprs[k]->semant( env );
		ConstNode *c=e->constNode();
		if( !c ) ex( "Blitz array sizes must be constant" );
		int n=c->intValue();
		if( n<0 ) ex( "Blitz array sizes must not be negative" );
		sizes.push_back( n+1 );
	}
	string label=genLabel();
	sem_type=d_new VectorType( label,ty,sizes );
	if( !d->insertDecl( ident,sem_type,kind ) ){
		delete sem_type;ex( "Duplicate identifier" );
	}
	env->types.push_back( sem_type );
}

void VectorDeclNode::translate( Codegen *g ){
	//type tag!
	g->align_data( 4 );
	VectorType *v=sem_type->vectorType();
	g->i_data( 6,v->label );
	int sz=1;
	for( int k=0;k<v->sizes.size();++k ) sz*=v->sizes[k];
	g->i_data( sz );
	string t;
	Type *type=v->elementType;
	if( type==Type::int_type ) t="__bbIntType";
	else if( type==Type::float_type ) t="__bbFltType";
	else if( type==Type::string_type ) t="__bbStrType";
	else if( StructType *s=type->structType() ) t="_t"+s->ident;
	else if( VectorType *v=type->vectorType() ) t=v->label;
	g->p_data( t );

	if( kind==DECL_GLOBAL ) g->i_data( 0,"_v"+ident );
}
