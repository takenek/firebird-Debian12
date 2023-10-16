/*
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Adriano dos Santos Fernandes
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2008 Adriano dos Santos Fernandes <adrianosf@uol.com.br>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef DSQL_DDL_NODES_H
#define DSQL_DDL_NODES_H

#include <optional>
#include "firebird/impl/blr.h"
#include "../jrd/dyn.h"
#include "../common/msg_encode.h"
#include "../dsql/make_proto.h"
#include "../dsql/BlrDebugWriter.h"
#include "../dsql/Nodes.h"
#include "../dsql/ExprNodes.h"
#include "../dsql/NodePrinter.h"
#include "../common/classes/array.h"
#include "../common/classes/ByteChunk.h"
#include "../common/classes/TriState.h"
#include "../jrd/Savepoint.h"
#include "../dsql/errd_proto.h"

namespace Jrd {

enum SqlSecurity
{
	SS_INVOKER,
	SS_DEFINER,
	SS_DROP
};

class LocalDeclarationsNode;
class RelationSourceNode;
class ValueListNode;
class SecDbContext;

class BoolSourceClause : public Printable
{
public:
	explicit BoolSourceClause(MemoryPool& p)
		: value(NULL),
		  source(p)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const
	{
		NODE_PRINT(printer, value);
		NODE_PRINT(printer, source);

		return "BoolSourceClause";
	}

public:
	NestConst<BoolExprNode> value;
	Firebird::string source;
};


class ValueSourceClause : public Printable
{
public:
	explicit ValueSourceClause(MemoryPool& p)
		: value(NULL),
		  source(p)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const
	{
		NODE_PRINT(printer, value);
		NODE_PRINT(printer, source);

		return "ValueSourceClause";
	}

public:
	NestConst<ValueExprNode> value;
	Firebird::string source;
};


class DbFileClause : public Printable
{
public:
	DbFileClause(MemoryPool& p, const DbFileClause& o)
		: name(p, o.name),
		  start(o.start),
		  length(o.length)
	{
	}

	explicit DbFileClause(MemoryPool& p, const Firebird::string& aName)
		: name(p, aName),
		  start(0),
		  length(0)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const
	{
		NODE_PRINT(printer, name);
		NODE_PRINT(printer, start);
		NODE_PRINT(printer, length);

		return "DbFileClause";
	}

public:
	Firebird::string name;	// File name
	SLONG start;			// Starting page
	SLONG length;			// File length in pages
};


class ExternalClause : public Printable
{
public:
	ExternalClause(MemoryPool& p, const ExternalClause& o)
		: name(p, o.name),
		  engine(p, o.engine),
		  udfModule(p, o.udfModule)
	{
	}

	explicit ExternalClause(MemoryPool& p)
		: name(p),
		  engine(p),
		  udfModule(p)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const
	{
		NODE_PRINT(printer, name);
		NODE_PRINT(printer, engine);
		NODE_PRINT(printer, udfModule);

		return "ExternalClause";
	}

public:
	Firebird::string name;
	MetaName engine;
	Firebird::string udfModule;
};


class ParameterClause : public Printable
{
public:
	ParameterClause(MemoryPool& pool, dsql_fld* field, const MetaName& aCollate,
		ValueSourceClause* aDefaultClause = NULL, ValueExprNode* aParameterExpr = NULL);

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;

public:
	MetaName name;
	NestConst<dsql_fld> type;
	NestConst<ValueSourceClause> defaultClause;
	NestConst<ValueExprNode> parameterExpr;
	std::optional<int> udfMechanism;
};


struct CollectedParameter
{
	CollectedParameter()
	{
		comment.clear();
		defaultSource.clear();
		defaultValue.clear();
	}

	bid comment;
	bid defaultSource;
	bid defaultValue;
};

typedef Firebird::GenericMap<
			Firebird::Pair<Firebird::Left<MetaName, CollectedParameter> > >
	CollectedParameterMap;


class ExecInSecurityDb
{
public:
	virtual ~ExecInSecurityDb() { }

	void executeInSecurityDb(jrd_tra* tra);

protected:
	virtual void runInSecurityDb(SecDbContext* secDbContext) = 0;
};


template <typename CreateNode, typename DropNode, ISC_STATUS ERROR_CODE>
class RecreateNode : public DdlNode
{
public:
	RecreateNode(MemoryPool& p, CreateNode* aCreateNode)
		: DdlNode(p),
		  createNode(aCreateNode),
		  dropNode(p, createNode->name)
	{
		dropNode.silent = true;
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const
	{
		Node::internalPrint(printer);

		NODE_PRINT(printer, createNode);
		NODE_PRINT(printer, dropNode);

		return "RecreateNode";
	}

	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction)
	{
		dropNode.checkPermission(tdbb, transaction);
		createNode->checkPermission(tdbb, transaction);
	}

	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction)
	{
		// run all statements under savepoint control
		AutoSavePoint savePoint(tdbb, transaction);

		dropNode.execute(tdbb, dsqlScratch, transaction);
		createNode->execute(tdbb, dsqlScratch, transaction);

		savePoint.release();	// everything is ok
	}

	virtual DdlNode* dsqlPass(DsqlCompilerScratch* dsqlScratch)
	{
		createNode->dsqlPass(dsqlScratch);
		dropNode.dsqlPass(dsqlScratch);
		return DdlNode::dsqlPass(dsqlScratch);
	}

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(ERROR_CODE) << createNode->name;
	}

protected:
	NestConst<CreateNode> createNode;
	DropNode dropNode;
};


class AlterCharSetNode : public DdlNode
{
public:
	AlterCharSetNode(MemoryPool& pool, const MetaName& aCharSet,
				const MetaName& aDefaultCollation)
		: DdlNode(pool),
		  charSet(pool, aCharSet),
		  defaultCollation(pool, aDefaultCollation)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_alter_charset_failed) << charSet;
	}

private:
	MetaName charSet;
	MetaName defaultCollation;
};


class AlterEDSPoolSetNode : public DdlNode
{
public:
	enum PARAM { POOL_SIZE, POOL_LIFETIME };

	AlterEDSPoolSetNode(MemoryPool& pool, PARAM prm, int val)
		: DdlNode(pool),
		  m_param(prm),
		  m_value(val)
	{
	}

public:
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

	virtual bool mustBeReplicated() const
	{
		return false;
	}

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		// TODO: statusVector << Firebird::Arg::Gds(??);
	}

private:
	PARAM m_param;
	int m_value;
};


class AlterEDSPoolClearNode : public DdlNode
{
public:
	enum PARAM { POOL_ALL, POOL_OLDEST, POOL_DB };

	AlterEDSPoolClearNode(MemoryPool& pool, PARAM prm, const Firebird::string& val = "")
		: DdlNode(pool),
		  m_param(prm),
		  m_value(pool)
	{
		m_value = val;
	}

public:
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

	virtual bool mustBeReplicated() const
	{
		return false;
	}

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		// TODO: statusVector << Firebird::Arg::Gds(??);
	}

private:
	PARAM m_param;
	Firebird::string m_value;
};


class CommentOnNode : public DdlNode
{
public:
	CommentOnNode(MemoryPool& pool, int aObjType,
				const QualifiedName& aObjName, const MetaName& aSubName,
				const Firebird::string aText)
		: DdlNode(pool),
		  objType(aObjType),
		  objName(pool, aObjName),
		  subName(pool, aSubName),
		  text(pool, aText),
		  str(pool)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		str = objName.toString();

		if (subName.hasData())
			str.append(".").append(subName.c_str());

		statusVector << Firebird::Arg::Gds(isc_dsql_comment_on_failed) << str;
	}

private:
	int objType;
	QualifiedName objName;
	MetaName subName;
	Firebird::string text, str;
};


class CreateAlterFunctionNode : public DdlNode
{
public:
	CreateAlterFunctionNode(MemoryPool& pool, const MetaName& aName)
		: DdlNode(pool),
		  name(pool, aName),
		  create(true),
		  alter(false),
		  external(NULL),
		  parameters(pool),
		  returnType(NULL),
		  localDeclList(NULL),
		  source(pool),
		  body(NULL),
		  compiled(false),
		  invalid(false),
		  package(pool),
		  packageOwner(pool),
		  privateScope(false),
		  preserveDefaults(false),
		  udfReturnPos(0)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual DdlNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector <<
			Firebird::Arg::Gds(createAlterCode(create, alter,
					isc_dsql_create_func_failed, isc_dsql_alter_func_failed,
					isc_dsql_create_alter_func_failed)) <<
				name;
	}

private:
	bool isUdf() const
	{
		return external && external->udfModule.hasData();
	}

	void executeCreate(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);
	bool executeAlter(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction,
		bool secondPass, bool runTriggers);
	bool executeAlterIndividualParameters(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction,
		bool secondPass, bool runTriggers);

	void storeArgument(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction,
		unsigned pos, bool returnArg, ParameterClause* parameter,
		const CollectedParameter* collectedParameter);
	void compile(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch);
	void collectParameters(thread_db* tdbb, jrd_tra* transaction, CollectedParameterMap& items);

public:
	MetaName name;
	bool create;
	bool alter;
	NestConst<ExternalClause> external;
	TriState deterministic;
	Firebird::Array<NestConst<ParameterClause> > parameters;
	NestConst<ParameterClause> returnType;
	NestConst<LocalDeclarationsNode> localDeclList;
	Firebird::string source;
	NestConst<StmtNode> body;
	bool compiled;
	bool invalid;
	MetaName package;
	MetaName packageOwner;
	bool privateScope;
	bool preserveDefaults;
	SLONG udfReturnPos;
	std::optional<SqlSecurity> ssDefiner;
};


class AlterExternalFunctionNode : public DdlNode
{
public:
	AlterExternalFunctionNode(MemoryPool& p, const MetaName& aName)
		: DdlNode(p),
		  name(p, aName),
		  clauses(p)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_alter_func_failed) << name;
	}

public:
	MetaName name;
	ExternalClause clauses;
};


class DropFunctionNode : public DdlNode
{
public:
	DropFunctionNode(MemoryPool& pool, const MetaName& aName)
		: DdlNode(pool),
		  name(pool, aName),
		  silent(false),
		  package(pool)
	{
	}

public:
	static void dropArguments(thread_db* tdbb, jrd_tra* transaction,
		const MetaName& functionName, const MetaName& packageName);

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual DdlNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_drop_func_failed) << name;
	}

public:
	MetaName name;
	bool silent;
	MetaName package;
};


typedef RecreateNode<CreateAlterFunctionNode, DropFunctionNode, isc_dsql_recreate_func_failed>
	RecreateFunctionNode;


class CreateAlterProcedureNode : public DdlNode
{
public:
	CreateAlterProcedureNode(MemoryPool& pool, const MetaName& aName)
		: DdlNode(pool),
		  name(pool, aName),
		  create(true),
		  alter(false),
		  external(NULL),
		  parameters(pool),
		  returns(pool),
		  source(pool),
		  localDeclList(NULL),
		  body(NULL),
		  compiled(false),
		  invalid(false),
		  package(pool),
		  packageOwner(pool),
		  privateScope(false),
		  preserveDefaults(false)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual DdlNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector <<
			Firebird::Arg::Gds(createAlterCode(create, alter,
					isc_dsql_create_proc_failed, isc_dsql_alter_proc_failed,
					isc_dsql_create_alter_proc_failed)) <<
				name;
	}

private:
	void executeCreate(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);
	bool executeAlter(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction,
		bool secondPass, bool runTriggers);
	void storeParameter(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction,
		USHORT parameterType, unsigned pos, ParameterClause* parameter,
		const CollectedParameter* collectedParameter);
	void compile(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch);
	void collectParameters(thread_db* tdbb, jrd_tra* transaction, CollectedParameterMap& items);

public:
	MetaName name;
	bool create;
	bool alter;
	NestConst<ExternalClause> external;
	Firebird::Array<NestConst<ParameterClause> > parameters;
	Firebird::Array<NestConst<ParameterClause> > returns;
	Firebird::string source;
	NestConst<LocalDeclarationsNode> localDeclList;
	NestConst<StmtNode> body;
	bool compiled;
	bool invalid;
	MetaName package;
	MetaName packageOwner;
	bool privateScope;
	bool preserveDefaults;
	TriState ssDefiner;
};


class DropProcedureNode : public DdlNode
{
public:
	DropProcedureNode(MemoryPool& pool, const MetaName& aName)
		: DdlNode(pool),
		  name(pool, aName),
		  silent(false),
		  package(pool)
	{
	}

public:
	static void dropParameters(thread_db* tdbb, jrd_tra* transaction,
		const MetaName& procedureName, const MetaName& packageName);

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual DdlNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_drop_proc_failed) << name;
	}

public:
	MetaName name;
	bool silent;
	MetaName package;
};


typedef RecreateNode<CreateAlterProcedureNode, DropProcedureNode, isc_dsql_recreate_proc_failed>
	RecreateProcedureNode;


class TriggerDefinition
{
public:

	explicit TriggerDefinition(MemoryPool& p)
		: name(p),
		  relationName(p),
		  external(NULL),
		  source(p),
		  systemFlag(fb_sysflag_user),
		  fkTrigger(false)
	{
	}

	void store(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);
	bool modify(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

	virtual ~TriggerDefinition()
	{
	}

protected:
	virtual void preModify(thread_db* /*tdbb*/, DsqlCompilerScratch* /*dsqlScratch*/,
		jrd_tra* /*transaction*/)
	{
	}

	virtual void postModify(thread_db* /*tdbb*/, DsqlCompilerScratch* /*dsqlScratch*/,
		jrd_tra* /*transaction*/)
	{
	}

public:
	MetaName name;
	MetaName relationName;
	std::optional<FB_UINT64> type;
	TriState active;
	std::optional<int> position;
	NestConst<ExternalClause> external;
	Firebird::string source;
	Firebird::ByteChunk blrData;
	Firebird::ByteChunk debugData;
	USHORT systemFlag;
	bool fkTrigger;
	std::optional<SqlSecurity> ssDefiner;
};


class CreateAlterTriggerNode : public DdlNode, public TriggerDefinition
{
public:
	CreateAlterTriggerNode(MemoryPool& p, const MetaName& aName)
		: DdlNode(p),
		  TriggerDefinition(p),
		  create(true),
		  alter(false),
		  localDeclList(NULL),
		  body(NULL),
		  compiled(false),
		  invalid(false)
	{
		name = aName;
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual DdlNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector <<
			Firebird::Arg::Gds(createAlterCode(create, alter,
					isc_dsql_create_trigger_failed, isc_dsql_alter_trigger_failed,
					isc_dsql_create_alter_trigger_failed)) <<
				name;
	}

	virtual void preModify(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction)
	{
		if (alter)
		{
			executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_BEFORE,
				DDL_TRIGGER_ALTER_TRIGGER, name, NULL);
		}
	}

	virtual void postModify(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction)
	{
		if (alter)
		{
			executeDdlTrigger(tdbb, dsqlScratch, transaction, DTW_AFTER,
				DDL_TRIGGER_ALTER_TRIGGER, name, NULL);
		}
	}

private:
	void executeCreate(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);
	void compile(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch);

	static inline bool hasOldContext(const unsigned value)
	{
		const unsigned val1 = ((value + 1) >> 1) & 3;
		const unsigned val2 = ((value + 1) >> 3) & 3;
		const unsigned val3 = ((value + 1) >> 5) & 3;
		return (val1 && val1 != 1) || (val2 && val2 != 1) || (val3 && val3 != 1);
	}

	static inline bool hasNewContext(const unsigned value)
	{
		const unsigned val1 = ((value + 1) >> 1) & 3;
		const unsigned val2 = ((value + 1) >> 3) & 3;
		const unsigned val3 = ((value + 1) >> 5) & 3;
		return (val1 && val1 != 3) || (val2 && val2 != 3) || (val3 && val3 != 3);
	}

public:
	bool create;
	bool alter;
	NestConst<LocalDeclarationsNode> localDeclList;
	NestConst<StmtNode> body;
	bool compiled;
	bool invalid;
};


class DropTriggerNode : public DdlNode
{
public:
	DropTriggerNode(MemoryPool& p, const MetaName& aName)
		: DdlNode(p),
		  name(p, aName),
		  silent(false)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual DdlNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_drop_trigger_failed) << name;
	}

public:
	MetaName name;
	bool silent;
};


typedef RecreateNode<CreateAlterTriggerNode, DropTriggerNode, isc_dsql_recreate_trigger_failed>
	RecreateTriggerNode;


class CreateCollationNode : public DdlNode
{
public:
	CreateCollationNode(MemoryPool& p, const MetaName& aName,
				const MetaName& aForCharSet)
		: DdlNode(p),
		  name(p, aName),
		  forCharSet(p, aForCharSet),
		  fromName(p),
		  fromExternal(p),
		  specificAttributes(p),
		  attributesOn(0),
		  attributesOff(0),
		  forCharSetId(0),
		  fromCollationId(0)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual DdlNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

	void setAttribute(USHORT attribute)
	{
		if ((attributesOn | attributesOff) & attribute)
		{
			// msg: 222: "Invalid collation attributes"
			using namespace Firebird;
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) << Arg::PrivateDyn(222));
		}

		attributesOn |= attribute;
	}

	void unsetAttribute(USHORT attribute)
	{
		if ((attributesOn | attributesOff) & attribute)
		{
			// msg: 222: "Invalid collation attributes"
			using namespace Firebird;
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) << Arg::PrivateDyn(222));
		}

		attributesOff |= attribute;
	}

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_create_collation_failed) << name;
	}

public:
	MetaName name;
	MetaName forCharSet;
	MetaName fromName;
	Firebird::string fromExternal;
	Firebird::UCharBuffer specificAttributes;

private:
	USHORT attributesOn;
	USHORT attributesOff;
	USHORT forCharSetId;
	USHORT fromCollationId;
};


class DropCollationNode : public DdlNode
{
public:
	DropCollationNode(MemoryPool& p, const MetaName& aName)
		: DdlNode(p),
		  name(p, aName)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_drop_collation_failed) << name;
	}

public:
	MetaName name;
};


class CreateDomainNode : public DdlNode
{
public:
	CreateDomainNode(MemoryPool& p, ParameterClause* aNameType)
		: DdlNode(p),
		  nameType(aNameType),
		  notNull(false),
		  check(NULL)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_create_domain_failed) << nameType->name;
	}

public:
	NestConst<ParameterClause> nameType;
	bool notNull;
	NestConst<BoolSourceClause> check;
};


class AlterDomainNode : public DdlNode
{
public:
	AlterDomainNode(MemoryPool& p, const MetaName& aName)
		: DdlNode(p),
		  name(p, aName),
		  dropConstraint(false),
		  dropDefault(false),
		  setConstraint(NULL),
		  setDefault(NULL),
		  renameTo(p)
	{
	}

public:
	static void checkUpdate(const dyn_fld& origFld, const dyn_fld& newFld);
	static ULONG checkUpdateNumericType(const dyn_fld& origFld, const dyn_fld& newFld);
	static void getDomainType(thread_db* tdbb, jrd_tra* transaction, dyn_fld& dynFld);
	static void modifyLocalFieldIndex(thread_db* tdbb, jrd_tra* transaction,
		const MetaName& relationName, const MetaName& fieldName,
		const MetaName& newFieldName);

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_alter_domain_failed) << name;
	}

private:
	void rename(thread_db* tdbb, jrd_tra* transaction, SSHORT dimensions);

public:
	MetaName name;
	bool dropConstraint;
	bool dropDefault;
	NestConst<BoolSourceClause> setConstraint;
	NestConst<ValueSourceClause> setDefault;
	MetaName renameTo;
	Firebird::AutoPtr<dsql_fld> type;
	TriState notNullFlag;	// true = NOT NULL / false = NULL
};


class DropDomainNode : public DdlNode
{
public:
	DropDomainNode(MemoryPool& p, const MetaName& aName)
		: DdlNode(p),
		  name(p, aName)
	{
	}

	static bool deleteDimensionRecords(thread_db* tdbb, jrd_tra* transaction,
		const MetaName& name);

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_drop_domain_failed) << name;
	}

private:
	void check(thread_db* tdbb, jrd_tra* transaction);

public:
	MetaName name;
};


class CreateAlterExceptionNode : public DdlNode
{
public:
	CreateAlterExceptionNode(MemoryPool& p, const MetaName& aName,
				const Firebird::string& aMessage)
		: DdlNode(p),
		  name(p, aName),
		  message(p, aMessage),
		  create(true),
		  alter(false)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector <<
			Firebird::Arg::Gds(createAlterCode(create, alter,
					isc_dsql_create_except_failed, isc_dsql_alter_except_failed,
					isc_dsql_create_alter_except_failed)) <<
				name;
	}

private:
	void executeCreate(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);
	bool executeAlter(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

public:
	MetaName name;
	Firebird::string message;
	bool create;
	bool alter;
};


class DropExceptionNode : public DdlNode
{
public:
	DropExceptionNode(MemoryPool& p, const MetaName& aName)
		: DdlNode(p),
		  name(p, aName),
		  silent(false)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_drop_except_failed) << name;
	}

public:
	MetaName name;
	bool silent;
};


typedef RecreateNode<CreateAlterExceptionNode, DropExceptionNode, isc_dsql_recreate_except_failed>
	RecreateExceptionNode;


class CreateAlterSequenceNode : public DdlNode
{
public:
	CreateAlterSequenceNode(MemoryPool& pool, const MetaName& aName)
		: DdlNode(pool),
		  create(true),
		  alter(false),
		  legacy(false),
		  restartSpecified(false),
		  name(pool, aName)
	{
		// Unfortunately, line/column carry no useful information here.
		// Hence, the check was created directly in parse.y.
		/*
		if (!aValue.specified && !aStep.specified)
		{
			using namespace Firebird;
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					  // Unexpected end of command
					  Arg::Gds(isc_command_end_err2) << Arg::Num(this->line) <<
													Arg::Num(this->column));
		}
		*/
	}

	static SSHORT store(thread_db* tdbb, jrd_tra* transaction, const MetaName& name,
		fb_sysflag sysFlag, SINT64 value, SLONG step);

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

	virtual DdlNode* dsqlPass(DsqlCompilerScratch* dsqlScratch)
	{
		dsqlScratch->getDsqlStatement()->setType(
			legacy ? DsqlStatement::TYPE_SET_GENERATOR : DsqlStatement::TYPE_DDL);
		return this;
	}

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector);

private:
	void executeCreate(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);
	bool executeAlter(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

public:
	bool create;
	bool alter;
	bool legacy;
	bool restartSpecified;
	const MetaName name;
	std::optional<SINT64> value;
	std::optional<SLONG> step;
};


class DropSequenceNode : public DdlNode
{
public:
	DropSequenceNode(MemoryPool& pool, const MetaName& aName)
		: DdlNode(pool),
		  name(pool, aName),
		  silent(false)
	{
	}

	static void deleteIdentity(thread_db* tdbb, jrd_tra* transaction,
		const MetaName& name);

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_drop_sequence_failed) << name;
	}

public:
	MetaName name;
	bool silent;
};


typedef RecreateNode<CreateAlterSequenceNode, DropSequenceNode, isc_dsql_recreate_sequence_failed>
	RecreateSequenceNode;


class RelationNode : public DdlNode
{
public:
	class FieldDefinition
	{
	public:
		explicit FieldDefinition(MemoryPool& p)
			: name(p),
			  relationName(p),
			  fieldSource(p),
			  identitySequence(p),
			  defaultSource(p),
			  baseField(p)
		{
		}

		void modify(thread_db* tdbb, jrd_tra* transaction);
		void store(thread_db* tdbb, jrd_tra* transaction);

	public:
		MetaName name;
		MetaName relationName;
		MetaName fieldSource;
		MetaName identitySequence;
		std::optional<IdentityType> identityType;
		std::optional<USHORT> collationId;
		TriState notNullFlag;	// true = NOT NULL / false = NULL
		std::optional<USHORT> position;
		Firebird::string defaultSource;
		Firebird::ByteChunk defaultValue;
		std::optional<USHORT> viewContext;
		MetaName baseField;
	};

	struct IndexConstraintClause
	{
		explicit IndexConstraintClause(MemoryPool& p)
			: name(p),
			  descending(false)
		{
		}

		MetaName name;
		bool descending;
	};

	struct Constraint
	{
		enum Type { TYPE_CHECK, TYPE_NOT_NULL, TYPE_PK, TYPE_UNIQUE, TYPE_FK };

		// Specialized BlrWriter for constraints.
		class BlrWriter : public Jrd::BlrDebugWriter
		{
		public:
			explicit BlrWriter(MemoryPool& p)
				: Jrd::BlrDebugWriter(p),
				  dsqlScratch(NULL)
			{
			}

			void init(DsqlCompilerScratch* aDsqlScratch)
			{
				dsqlScratch = aDsqlScratch;
				dsqlScratch->getBlrData().clear();
				dsqlScratch->getDebugData().clear();
				appendUChar(isVersion4() ? blr_version4 : blr_version5);
			}

			virtual bool isVersion4()
			{
				return dsqlScratch->isVersion4();
			}

		private:
			DsqlCompilerScratch* dsqlScratch;
		};

		explicit Constraint(MemoryPool& p)
			: type(TYPE_CHECK),	// Just something to initialize. Do not assume it.
			  columns(p),
			  index(NULL),
			  refRelation(p),
			  refColumns(p),
			  refUpdateAction(RI_RESTRICT),
			  refDeleteAction(RI_RESTRICT),
			  triggers(p),
			  blrWritersHolder(p)
		{
		}

		Constraint::Type type;
		Firebird::ObjectsArray<MetaName> columns;
		NestConst<IndexConstraintClause> index;
		MetaName refRelation;
		Firebird::ObjectsArray<MetaName> refColumns;
		const char* refUpdateAction;
		const char* refDeleteAction;
		Firebird::ObjectsArray<TriggerDefinition> triggers;
		Firebird::ObjectsArray<BlrWriter> blrWritersHolder;
	};

	struct CreateDropConstraint
	{
		explicit CreateDropConstraint(MemoryPool& p)
			: name(p)
		{
		}

		MetaName name;
		Firebird::AutoPtr<Constraint> create;
	};

	struct Clause
	{
		enum Type
		{
			TYPE_ADD_CONSTRAINT,
			TYPE_ADD_COLUMN,
			TYPE_ALTER_COL_NAME,
			TYPE_ALTER_COL_NULL,
			TYPE_ALTER_COL_POS,
			TYPE_ALTER_COL_TYPE,
			TYPE_DROP_COLUMN,
			TYPE_DROP_CONSTRAINT,
			TYPE_ALTER_SQL_SECURITY,
			TYPE_ALTER_PUBLICATION
		};

		explicit Clause(MemoryPool& p, Type aType)
			: type(aType)
		{
		}

		const Type type;
	};

	struct RefActionClause
	{
		static const unsigned ACTION_CASCADE		= 1;
		static const unsigned ACTION_SET_DEFAULT	= 2;
		static const unsigned ACTION_SET_NULL		= 3;
		static const unsigned ACTION_NONE			= 4;

		RefActionClause(MemoryPool& p, unsigned aUpdateAction, unsigned aDeleteAction)
			: updateAction(aUpdateAction),
			  deleteAction(aDeleteAction)
		{
		}

		unsigned updateAction;
		unsigned deleteAction;
	};

	struct AddConstraintClause : public Clause
	{
		enum ConstraintType
		{
			CTYPE_NOT_NULL,
			CTYPE_PK,
			CTYPE_FK,
			CTYPE_UNIQUE,
			CTYPE_CHECK
		};

		explicit AddConstraintClause(MemoryPool& p)
			: Clause(p, TYPE_ADD_CONSTRAINT),
			  name(p),
			  constraintType(CTYPE_NOT_NULL),
			  columns(p),
			  index(NULL),
			  refRelation(p),
			  refColumns(p),
			  refAction(NULL),
			  check(NULL)
		{
		}

		MetaName name;
		ConstraintType constraintType;
		Firebird::ObjectsArray<MetaName> columns;
		NestConst<IndexConstraintClause> index;
		MetaName refRelation;
		Firebird::ObjectsArray<MetaName> refColumns;
		NestConst<RefActionClause> refAction;
		NestConst<BoolSourceClause> check;
	};

	struct IdentityOptions
	{
		IdentityOptions(MemoryPool&, IdentityType aType)
			: type(aType),
			  restart(false)
		{
		}

		IdentityOptions(MemoryPool&)
			: restart(false)
		{
		}

		std::optional<IdentityType> type;
		std::optional<SINT64> startValue;
		std::optional<SLONG> increment;
		bool restart;	// used in ALTER
	};

	struct AddColumnClause : public Clause
	{
		explicit AddColumnClause(MemoryPool& p)
			: Clause(p, TYPE_ADD_COLUMN),
			  field(NULL),
			  defaultValue(NULL),
			  constraints(p),
			  collate(p),
			  computed(NULL),
			  identityOptions(NULL),
			  notNullSpecified(false)
		{
		}

		dsql_fld* field;
		NestConst<ValueSourceClause> defaultValue;
		Firebird::ObjectsArray<AddConstraintClause> constraints;
		MetaName collate;
		NestConst<ValueSourceClause> computed;
		NestConst<IdentityOptions> identityOptions;
		bool notNullSpecified;
	};

	struct AlterColNameClause : public Clause
	{
		explicit AlterColNameClause(MemoryPool& p)
			: Clause(p, TYPE_ALTER_COL_NAME),
			  fromName(p),
			  toName(p)
		{
		}

		MetaName fromName;
		MetaName toName;
	};

	struct AlterColNullClause : public Clause
	{
		explicit AlterColNullClause(MemoryPool& p)
			: Clause(p, TYPE_ALTER_COL_NULL),
			  name(p),
			  notNullFlag(false)
		{
		}

		MetaName name;
		bool notNullFlag;
	};

	struct AlterColPosClause : public Clause
	{
		explicit AlterColPosClause(MemoryPool& p)
			: Clause(p, TYPE_ALTER_COL_POS),
			  name(p),
			  newPos(0)
		{
		}

		MetaName name;
		SSHORT newPos;
	};

	struct AlterColTypeClause : public Clause
	{
		explicit AlterColTypeClause(MemoryPool& p)
			: Clause(p, TYPE_ALTER_COL_TYPE),
			  field(NULL),
			  defaultValue(NULL),
			  dropDefault(false),
			  dropIdentity(false),
			  identityOptions(NULL),
			  computed(NULL)
		{
		}

		dsql_fld* field;
		NestConst<ValueSourceClause> defaultValue;
		bool dropDefault;
		bool dropIdentity;
		NestConst<IdentityOptions> identityOptions;
		NestConst<ValueSourceClause> computed;
	};

	struct DropColumnClause : public Clause
	{
		explicit DropColumnClause(MemoryPool& p)
			: Clause(p, TYPE_DROP_COLUMN),
			  name(p),
			  cascade(false)
		{
		}

		MetaName name;
		bool cascade;
	};

	struct DropConstraintClause : public Clause
	{
		explicit DropConstraintClause(MemoryPool& p)
			: Clause(p, TYPE_DROP_CONSTRAINT),
			  name(p)
		{
		}

		MetaName name;
	};

	RelationNode(MemoryPool& p, RelationSourceNode* aDsqlNode);

	static void deleteLocalField(thread_db* tdbb, jrd_tra* transaction,
		const MetaName& relationName, const MetaName& fieldName);

	static void addToPublication(thread_db* tdbb, jrd_tra* transaction,
		const MetaName& tableName, const MetaName& pubTame);
	static void dropFromPublication(thread_db* tdbb, jrd_tra* transaction,
		const MetaName& tableName, const MetaName& pubTame);

protected:
	virtual Firebird::string internalPrint(NodePrinter& printer) const
	{
		DdlNode::internalPrint(printer);

		//// FIXME-PRINT: NODE_PRINT(printer, dsqlNode);
		NODE_PRINT(printer, name);
		//// FIXME-PRINT: NODE_PRINT(printer, clauses);

		return "RelationNode";
	}

	void defineField(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction,
		AddColumnClause* clause, SSHORT position,
		const Firebird::ObjectsArray<MetaName>* pkcols);
	bool defineDefault(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, dsql_fld* field,
		ValueSourceClause* clause, Firebird::string& source, BlrDebugWriter::BlrData& value);
	void makeConstraint(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction,
		AddConstraintClause* clause, Firebird::ObjectsArray<CreateDropConstraint>& constraints,
		bool* notNull = NULL);
	void defineConstraint(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction,
		MetaName& constraintName, Constraint& constraint);
	void defineCheckConstraint(DsqlCompilerScratch* dsqlScratch, Constraint& constraint,
		BoolSourceClause* clause);
	void defineCheckConstraintTrigger(DsqlCompilerScratch* dsqlScratch, Constraint& constraint,
		BoolSourceClause* clause, FB_UINT64 triggerType);
	void defineSetDefaultTrigger(DsqlCompilerScratch* dsqlScratch, Constraint& constraint,
		bool onUpdate);
	void defineSetNullTrigger(DsqlCompilerScratch* dsqlScratch, Constraint& constraint,
		bool onUpdate);
	void defineDeleteCascadeTrigger(DsqlCompilerScratch* dsqlScratch, Constraint& constraint);
	void defineUpdateCascadeTrigger(DsqlCompilerScratch* dsqlScratch, Constraint& constraint);
	void generateUnnamedTriggerBeginning(Constraint& constraint, bool onUpdate,
		BlrDebugWriter& blrWriter);
	void stuffDefaultBlr(const Firebird::ByteChunk& defaultBlr, BlrDebugWriter& blrWriter);
	void stuffMatchingBlr(Constraint& constraint, BlrDebugWriter& blrWriter);
	void stuffTriggerFiringCondition(const Constraint& constraint, BlrDebugWriter& blrWriter);

public:
	NestConst<RelationSourceNode> dsqlNode;
	MetaName name;
	Firebird::Array<NestConst<Clause> > clauses;
	TriState ssDefiner;
	TriState replicationState;
};


class CreateRelationNode : public RelationNode
{
public:
	CreateRelationNode(MemoryPool& p, RelationSourceNode* aDsqlNode,
				const Firebird::string* aExternalFile = NULL)
		: RelationNode(p, aDsqlNode),
		  externalFile(aExternalFile)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_create_table_failed) << name;
	}

private:
	const Firebird::ObjectsArray<MetaName>* findPkColumns();

public:
	const Firebird::string* externalFile;
	std::optional<rel_t> relationType = rel_persistent;
	bool preserveRowsOpt;
	bool deleteRowsOpt;
};


class AlterRelationNode : public RelationNode
{
public:
	AlterRelationNode(MemoryPool& p, RelationSourceNode* aDsqlNode)
		: RelationNode(p, aDsqlNode)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_alter_table_failed) << name;
	}

private:
	void modifyField(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction,
		AlterColTypeClause* clause);
};


class DropRelationNode : public DdlNode
{
public:
	DropRelationNode(MemoryPool& p, const MetaName& aName, bool aView = false)
		: DdlNode(p),
		  name(p, aName),
		  view(aView),
		  silent(false)
	{
	}

	static void deleteGlobalField(thread_db* tdbb, jrd_tra* transaction,
		const MetaName& globalName);

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(view ? isc_dsql_drop_view_failed :
			isc_dsql_drop_table_failed) << name;
	}

public:
	MetaName name;
	bool view;
	bool silent;
};


typedef RecreateNode<CreateRelationNode, DropRelationNode, isc_dsql_recreate_table_failed>
	RecreateTableNode;


class CreateAlterViewNode : public RelationNode
{
public:
	CreateAlterViewNode(MemoryPool& p, RelationSourceNode* aDsqlNode, ValueListNode* aViewFields,
				SelectExprNode* aSelectExpr)
		: RelationNode(p, aDsqlNode),
		  create(true),
		  alter(false),
		  viewFields(aViewFields),
		  selectExpr(aSelectExpr),
		  source(p),
		  withCheckOption(false)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual DdlNode* dsqlPass(DsqlCompilerScratch* dsqlScratch);
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector <<
			Firebird::Arg::Gds(createAlterCode(create, alter,
					isc_dsql_create_view_failed, isc_dsql_alter_view_failed,
					isc_dsql_create_alter_view_failed)) <<
				name;
	}

private:
	void createCheckTrigger(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, ValueListNode* items,
		TriggerType triggerType);

public:
	bool create;
	bool alter;
	NestConst<ValueListNode> viewFields;
	NestConst<SelectExprNode> selectExpr;
	Firebird::string source;
	bool withCheckOption;
};


class RecreateViewNode :
	public RecreateNode<CreateAlterViewNode, DropRelationNode, isc_dsql_recreate_view_failed>
{
public:
	RecreateViewNode(MemoryPool& p, CreateAlterViewNode* aCreateNode)
		: RecreateNode<CreateAlterViewNode, DropRelationNode, isc_dsql_recreate_view_failed>(
				p, aCreateNode)
	{
		dropNode.view = true;
	}
};


class CreateIndexNode : public DdlNode
{
public:
	struct Definition
	{
		Definition()
			: type(0)
		{
			expressionBlr.clear();
			expressionSource.clear();
			conditionBlr.clear();
			conditionSource.clear();
		}

		MetaName relation;
		Firebird::ObjectsArray<MetaName> columns;
		TriState unique;
		TriState descending;
		TriState inactive;
		SSHORT type;
		bid expressionBlr;
		bid expressionSource;
		bid conditionBlr;
		bid conditionSource;
		MetaName refRelation;
		Firebird::ObjectsArray<MetaName> refColumns;
	};

public:
	CreateIndexNode(MemoryPool& p, const MetaName& aName)
		: DdlNode(p),
		  name(p, aName),
		  unique(false),
		  descending(false),
		  relation(nullptr),
		  columns(nullptr),
		  computed(nullptr),
		  partial(nullptr)
	{
	}

public:
	static void store(thread_db* tdbb, jrd_tra* transaction, MetaName& name,
		const Definition& definition, MetaName* referredIndexName = nullptr);

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_create_index_failed) << name;
	}

public:
	MetaName name;
	bool unique;
	bool descending;
	NestConst<RelationSourceNode> relation;
	NestConst<ValueListNode> columns;
	NestConst<ValueSourceClause> computed;
	NestConst<BoolSourceClause> partial;
};


class AlterIndexNode : public DdlNode
{
public:
	AlterIndexNode(MemoryPool& p, const MetaName& aName, bool aActive)
		: DdlNode(p),
		  name(p, aName),
		  active(aActive)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_alter_index_failed) << name;
	}

public:
	MetaName name;
	bool active;
};


class SetStatisticsNode : public DdlNode
{
public:
	SetStatisticsNode(MemoryPool& p, const MetaName& aName)
		: DdlNode(p),
		  name(p, aName)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		// ASF: using ALTER INDEX's code.
		statusVector << Firebird::Arg::Gds(isc_dsql_alter_index_failed) << name;
	}

public:
	MetaName name;
};


class DropIndexNode : public DdlNode
{
public:
	DropIndexNode(MemoryPool& p, const MetaName& aName)
		: DdlNode(p),
		  name(p, aName)
	{
	}

	static bool deleteSegmentRecords(thread_db* tdbb, jrd_tra* transaction,
		const MetaName& name);

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_drop_index_failed) << name;
	}

public:
	MetaName name;
};


class CreateFilterNode : public DdlNode
{
public:
	class NameNumber : public Printable
	{
	public:
		NameNumber(MemoryPool& p, const MetaName& aName)
			: name(p, aName),
			  number(0)
		{
		}

		NameNumber(MemoryPool& p, SSHORT aNumber)
			: name(p),
			  number(aNumber)
		{
		}

	public:
		virtual Firebird::string internalPrint(NodePrinter& printer) const
		{
			NODE_PRINT(printer, name);
			NODE_PRINT(printer, number);

			return "NameNumber";
		}

	public:
		MetaName name;
		SSHORT number;
	};

public:
	CreateFilterNode(MemoryPool& p, const MetaName& aName)
		: DdlNode(p),
		  name(p, aName),
		  inputFilter(NULL),
		  outputFilter(NULL),
		  entryPoint(p),
		  moduleName(p)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_create_filter_failed) << name;
	}

public:
	MetaName name;
	NestConst<NameNumber> inputFilter;
	NestConst<NameNumber> outputFilter;
	Firebird::string entryPoint;
	Firebird::string moduleName;
};


class DropFilterNode : public DdlNode
{
public:
	DropFilterNode(MemoryPool& p, const MetaName& aName)
		: DdlNode(p),
		  name(p, aName)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_drop_filter_failed) << name;
	}

public:
	MetaName name;
};


class CreateShadowNode : public DdlNode
{
public:
	CreateShadowNode(MemoryPool& p, const SSHORT aNumber)
		: DdlNode(p),
		  number(aNumber),
		  manual(false),
		  conditional(false),
		  files(p)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

	virtual bool mustBeReplicated() const
	{
		return false;
	}

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_create_shadow_failed) << Firebird::Arg::Num(number);
	}

public:
	SSHORT number;
	bool manual;
	bool conditional;
	Firebird::Array<NestConst<DbFileClause> > files;
};


class DropShadowNode : public DdlNode
{
public:
	DropShadowNode(MemoryPool& p, const SSHORT aNumber, const bool aNodelete)
		: DdlNode(p),
		  number(aNumber),
		  nodelete(aNodelete)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

	virtual bool mustBeReplicated() const
	{
		return false;
	}

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_drop_shadow_failed) << Firebird::Arg::Num(number);
	}

public:
	SSHORT number;
	bool nodelete;
};


class PrivilegesNode : public DdlNode
{
public:
	PrivilegesNode(MemoryPool& p)
		: DdlNode(p)
	{
	}

protected:
	static USHORT convertPrivilegeFromString(thread_db* tdbb, jrd_tra* transaction,
		MetaName privilege);
};

class CreateAlterRoleNode : public PrivilegesNode
{
public:
	CreateAlterRoleNode(MemoryPool& p, const MetaName& aName)
		: PrivilegesNode(p),
		  name(p, aName),
		  createFlag(false),
		  sysPrivDrop(false),
		  privileges(p)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(createFlag ? isc_dsql_create_role_failed :
			isc_dsql_alter_role_failed) << name;
	}

private:
	bool isItUserName(thread_db* tdbb, jrd_tra* transaction);

public:
	MetaName name;
	bool createFlag, sysPrivDrop;

	void addPrivilege(const MetaName* privName)
	{
		fb_assert(privName);
		privileges.push(*privName);
	}

private:
	Firebird::HalfStaticArray<MetaName, 4> privileges;
};


class MappingNode : public DdlNode, private ExecInSecurityDb
{
public:
	enum OP {MAP_ADD, MAP_MOD, MAP_RPL, MAP_DROP, MAP_COMMENT};

	MappingNode(MemoryPool& p, OP o, const MetaName& nm)
		: DdlNode(p),
		  name(p, nm),
		  fromUtf8(p),
		  plugin(NULL),
		  db(NULL),
		  fromType(NULL),
		  from(NULL),
		  to(NULL),
		  comment(NULL),
		  op(o),
		  mode('#'),
		  global(false),
		  role(false)
	{
	}

	void validateAdmin();

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_mapping_failed) << name <<
			(op == MAP_ADD ? "CREATE" : op == MAP_MOD ?
			 "ALTER" : op == MAP_RPL ? "CREATE OR ALTER" : op == MAP_DROP ?
			 "DROP" : "COMMENT ON");
	}
	void runInSecurityDb(SecDbContext* secDbContext);

private:
	void addItem(Firebird::string& ddl, const char* text, char quote = '"');

public:
	MetaName name;
	Firebird::string fromUtf8;
	MetaName* plugin;
	MetaName* db;
	MetaName* fromType;
	IntlString* from;
	MetaName* to;
	Firebird::string* comment;
	OP op;
	char mode;	// * - any source, P - plugin, M - mapping, S - any serverwide plugin
	bool global;
	bool role;
};


class DropRoleNode : public DdlNode
{
public:
	DropRoleNode(MemoryPool& p, const MetaName& aName)
		: DdlNode(p),
		  name(p, aName)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_drop_role_failed) << name;
	}

public:
	MetaName name;
};


class UserNode : public DdlNode
{
public:
	explicit UserNode(MemoryPool& p)
		: DdlNode(p)
	{ }

protected:
	MetaName upper(const MetaName& str);
};


class CreateAlterUserNode : public UserNode
{
public:
	enum Mode {USER_ADD, USER_MOD, USER_RPL};

	CreateAlterUserNode(MemoryPool& p, Mode md, const MetaName& aName)
		: UserNode(p),
		  properties(p),
		  name(p, upper(aName)),
		  password(NULL),
		  firstName(NULL),
		  middleName(NULL),
		  lastName(NULL),
		  plugin(NULL),
		  comment(NULL),
		  mode(md)
	{ }

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

	virtual bool mustBeReplicated() const
	{
		return false;
	}

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(mode == USER_ADD ?
			isc_dsql_create_user_failed : isc_dsql_alter_user_failed) << name;
	}

public:
	class Property
	{
	public:
		explicit Property(MemoryPool& p)
			: value(p)
		{ }

		MetaName property;
		Firebird::string value;
	};

	Firebird::ObjectsArray<Property> properties;
	const MetaName name;
	Firebird::string* password;
	Firebird::string* firstName;
	Firebird::string* middleName;
	Firebird::string* lastName;
	MetaName* plugin;
	Firebird::string* comment;
	TriState adminRole;
	TriState active;
	Mode mode;

	void addProperty(MetaName* pr, Firebird::string* val = NULL)
	{
		fb_assert(pr);

		Property& p = properties.add();
		p.property = *pr;
		if (val)
		{
			p.value = *val;
		}
	}
};


class DropUserNode : public UserNode
{
public:
	DropUserNode(MemoryPool& p, const MetaName& aName, const MetaName* aPlugin = NULL)
		: UserNode(p),
		  name(p, upper(aName)),
		  plugin(p),
		  silent(false)
	{
		if (aPlugin)
			plugin = *aPlugin;
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

	virtual bool mustBeReplicated() const
	{
		return false;
	}

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_drop_user_failed) << name;
	}

public:
	const MetaName name;
	MetaName plugin;
	bool silent;
};


template <>
inline RecreateNode<CreateAlterUserNode, DropUserNode, isc_dsql_recreate_user_failed>::
	RecreateNode(MemoryPool& p, CreateAlterUserNode* aCreateNode)
		: DdlNode(p),
		  createNode(aCreateNode),
		  dropNode(p, createNode->name, createNode->plugin)
	{
		dropNode.silent = true;
	}

typedef RecreateNode<CreateAlterUserNode, DropUserNode, isc_dsql_recreate_user_failed>
	RecreateUserNode;


typedef Firebird::Pair<Firebird::NonPooled<char, ValueListNode*> > PrivilegeClause;
typedef Firebird::Pair<Firebird::NonPooled<ObjectType, MetaName> > GranteeClause;

class GrantRevokeNode : public PrivilegesNode, private ExecInSecurityDb
{
public:
	GrantRevokeNode(MemoryPool& p, bool aIsGrant)
		: PrivilegesNode(p),
		  createDbJobs(p),
		  isGrant(aIsGrant),
		  privileges(p),
		  roles(p),
		  defaultRoles(p),
		  object(NULL),
		  users(p),
		  grantAdminOption(false),
		  grantor(NULL),
		  isDdl(false)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector <<
			Firebird::Arg::Gds(isGrant ? isc_dsql_grant_failed : isc_dsql_revoke_failed);
	}
	void runInSecurityDb(SecDbContext* secDbContext);

private:
	void modifyPrivileges(thread_db* tdbb, jrd_tra* transaction, SSHORT option, const GranteeClause* user);
	void grantRevoke(thread_db* tdbb, jrd_tra* transaction, const GranteeClause* object,
		const GranteeClause* userNod, const char* privs, MetaName field, int options);
	static void checkGrantorCanGrantRelation(thread_db* tdbb, jrd_tra* transaction, const char* grantor,
		const char* privilege, const MetaName& relationName,
		const MetaName& fieldName, bool topLevel);
	static void checkGrantorCanGrantRole(thread_db* tdbb, jrd_tra* transaction,
			const MetaName& grantor, const MetaName& roleName);
	static void checkGrantorCanGrantDdl(thread_db* tdbb, jrd_tra* transaction,
			const MetaName& grantor, const char* privilege, const MetaName& objName);
	static void checkGrantorCanGrantObject(thread_db* tdbb, jrd_tra* transaction, const char* grantor,
		const char* privilege, const MetaName& objName, SSHORT objType);
	static void storePrivilege(thread_db* tdbb, jrd_tra* transaction,
		const MetaName& object, const MetaName& user,
		const MetaName& field, const TEXT* privilege, SSHORT userType,
		SSHORT objType, int option, const MetaName& grantor);
	static void setFieldClassName(thread_db* tdbb, jrd_tra* transaction,
		const MetaName& relation, const MetaName& field);

	// Diagnostics print helper.
	static const char* privilegeName(char symbol)
	{
		switch (UPPER7(symbol))
		{
			case 'A': return "ALL";
			case 'I': return "INSERT";
			case 'U': return "UPDATE";
			case 'D': return "DELETE";
			case 'S': return "SELECT";
			case 'X': return "EXECUTE";
			case 'G': return "USAGE";
			case 'M': return "ROLE";
			case 'R': return "REFERENCE";
			// ddl
			case 'C': return "CREATE";
			case 'L': return "ALTER";
			case 'O': return "DROP";
		}

		return "<Unknown>";
	}

	struct CreateDbJob
	{
		CreateDbJob(SSHORT a_userType, const MetaName& a_user)
			: allOnAll(false), grantErased(false), badGrantor(false),
			  userType(a_userType), user(a_user)
		{ }

		bool allOnAll, grantErased, badGrantor;
		SSHORT userType;
		MetaName user, revoker;
	};
	Firebird::Array<CreateDbJob> createDbJobs;

public:
	bool isGrant;
	Firebird::Array<PrivilegeClause> privileges;
	Firebird::Array<GranteeClause> roles;
	Firebird::Array<bool> defaultRoles;
	NestConst<GranteeClause> object;
	Firebird::Array<GranteeClause> users;
	bool grantAdminOption;
	NestConst<MetaName> grantor;
	// ddl rights
	bool isDdl;
};


class AlterDatabaseNode : public DdlNode
{
public:
	static const unsigned CLAUSE_BEGIN_BACKUP		= 0x01;
	static const unsigned CLAUSE_END_BACKUP			= 0x02;
	static const unsigned CLAUSE_DROP_DIFFERENCE	= 0x04;
	static const unsigned CLAUSE_CRYPT				= 0x08;
	static const unsigned CLAUSE_ENABLE_PUB			= 0x10;
	static const unsigned CLAUSE_DISABLE_PUB		= 0x20;
	static const unsigned CLAUSE_PUB_INCL_TABLE		= 0x40;
	static const unsigned CLAUSE_PUB_EXCL_TABLE		= 0x80;

	static const unsigned RDB_DATABASE_MASK =
		CLAUSE_BEGIN_BACKUP | CLAUSE_END_BACKUP | CLAUSE_DROP_DIFFERENCE;

public:
	AlterDatabaseNode(MemoryPool& p)
		: DdlNode(p),
		  create(false),
		  createLength(0),
		  linger(-1),
		  clauses(0),
		  differenceFile(p),
		  setDefaultCharSet(p),
		  setDefaultCollation(p),
		  files(p),
		  cryptPlugin(p),
		  keyName(p),
		  pubTables(p)
	{
	}

public:
	virtual DdlNode* dsqlPass(DsqlCompilerScratch* dsqlScratch)
	{
		dsqlScratch->getDsqlStatement()->setType(
			create ? DsqlStatement::TYPE_CREATE_DB : DsqlStatement::TYPE_DDL);
		return this;
	}

	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

	virtual bool mustBeReplicated() const
	{
		return false;
	}

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_alter_database_failed);
	}

private:
	static void changeBackupMode(thread_db* tdbb, jrd_tra* transaction, unsigned clause);
	static void defineDifference(thread_db* tdbb, jrd_tra* transaction, const Firebird::PathName& file);
	void checkClauses(thread_db* tdbb);

public:
	bool create;	// Is the node created with a CREATE DATABASE command?
	SLONG createLength, linger;
	unsigned clauses;
	Firebird::string differenceFile;
	MetaName setDefaultCharSet;
	MetaName setDefaultCollation;
	Firebird::Array<NestConst<DbFileClause> > files;
	MetaName cryptPlugin;
	MetaName keyName;
	TriState ssDefiner;
	Firebird::Array<MetaName> pubTables;
};


} // namespace

#endif // DSQL_DDL_NODES_H
