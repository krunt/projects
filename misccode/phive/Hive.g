grammar Hive;

options
{
	language=C;
	output=AST;
	ASTLabelType=pANTLR3_BASE_TREE;
	backtrack=true;
    memoize=true;
	k=3;
}
 
tokens {
TOK_INSERT;
TOK_QUERY;
TOK_SELECT;
TOK_SELECTDI;
TOK_SELEXPR;
TOK_FROM;
TOK_TAB;
TOK_PARTSPEC;
TOK_PARTVAL;
TOK_DIR;
TOK_LOCAL_DIR;
TOK_TABREF;
TOK_SUBQUERY;
TOK_DESTINATION;
TOK_ALLCOLREF;
TOK_COLREF;
TOK_FUNCTION;
TOK_FUNCTIONDI;
TOK_WHERE;
TOK_OP_EQ;
TOK_OP_NE;
TOK_OP_LE;
TOK_OP_LT;
TOK_OP_GE;
TOK_OP_GT;
TOK_OP_DIV;
TOK_OP_ADD;
TOK_OP_SUB;
TOK_OP_MUL;
TOK_OP_MOD;
TOK_OP_BITAND;
TOK_OP_BITNOT;
TOK_OP_BITOR;
TOK_OP_BITXOR;
TOK_OP_AND;
TOK_OP_OR;
TOK_OP_NOT;
TOK_OP_LIKE;
TOK_TRUE;
TOK_FALSE;
TOK_TRANSFORM;
TOK_EXPLIST;
TOK_ALIASLIST;
TOK_GROUPBY;
TOK_ORDERBY;
TOK_CLUSTERBY;
TOK_DISTRIBUTEBY;
TOK_SORTBY;
TOK_UNION;
TOK_JOIN;
TOK_LEFTOUTERJOIN;
TOK_RIGHTOUTERJOIN;
TOK_FULLOUTERJOIN;
TOK_LOAD;
TOK_NULL;
TOK_ISNULL;
TOK_ISNOTNULL;
TOK_TINYINT;
TOK_SMALLINT;
TOK_INT;
TOK_BIGINT;
TOK_BOOLEAN;
TOK_FLOAT;
TOK_DOUBLE;
TOK_DATE;
TOK_DATETIME;
TOK_TIMESTAMP;
TOK_STRING;
TOK_LIST;
TOK_MAP;
TOK_CREATETABLE;
TOK_DESCTABLE;
TOK_ALTERTABLE_RENAME;
TOK_ALTERTABLE_ADDCOLS;
TOK_ALTERTABLE_REPLACECOLS;
TOK_ALTERTABLE_ADDPARTS;
TOK_ALTERTABLE_DROPPARTS;
TOK_ALTERTABLE_SERDEPROPERTIES;
TOK_ALTERTABLE_SERIALIZER;
TOK_ALTERTABLE_PROPERTIES;
TOK_MSCK;
TOK_SHOWTABLES;
TOK_SHOWPARTITIONS;
TOK_CREATEEXTTABLE;
TOK_DROPTABLE;
TOK_TABCOLLIST;
TOK_TABCOL;
TOK_TABLECOMMENT;
TOK_TABLEPARTCOLS;
TOK_TABLEBUCKETS;
TOK_TABLEROWFORMAT;
TOK_TABLEROWFORMATFIELD;
TOK_TABLEROWFORMATCOLLITEMS;
TOK_TABLEROWFORMATMAPKEYS;
TOK_TABLEROWFORMATLINES;
TOK_TBLSEQUENCEFILE;
TOK_TBLTEXTFILE;
TOK_TABLEFILEFORMAT;
TOK_TABCOLNAME;
TOK_TABLELOCATION;
TOK_PARTITIONLOCATION;
TOK_TABLESAMPLE;
TOK_TMP_FILE;
TOK_TABSORTCOLNAMEASC;
TOK_TABSORTCOLNAMEDESC;
TOK_CHARSETLITERAL;
TOK_CREATEFUNCTION;
TOK_EXPLAIN;
TOK_TABLESERIALIZER;
TOK_TABLEPROPERTIES;
TOK_TABLEPROPLIST;
TOK_TABTYPE;
TOK_LIMIT;
TOK_TABLEPROPERTY;
TOK_IFNOTEXISTS;
TOK_FILTERLIST;
TOK_FILTERITEM;
TOK_PARAMS;
}


// starting rule
statement
	: execStatement EOF
	;

execStatement
    : queryStatementExpression
    ;
   
queryStatementExpression
    : queryStatement
    ;

queryStatement
    :
    fromClause
    ( b+=body )+ -> ^(TOK_QUERY fromClause body+)
    | regular_body
    ;

regular_body
   :
   insertClause
   selectClause
   fromClause
   whereClause?
   groupByClause? 
   orderByClause?
   limitClause?  -> ^(TOK_QUERY fromClause ^(TOK_INSERT insertClause
                          selectClause whereClause? groupByClause? orderByClause? limitClause?)) 
   |
   selectClause
   fromClause
   whereClause?
   groupByClause? 
   orderByClause?
   limitClause?  -> ^(TOK_QUERY fromClause ^(TOK_INSERT ^(TOK_DESTINATION ^(TOK_DIR TOK_TMP_FILE))
                          selectClause whereClause? groupByClause? orderByClause? limitClause?)) 
   ;

body
   :
   insertClause
   selectClause
   whereClause?
   groupByClause? 
   orderByClause?
   limitClause?  -> ^(TOK_INSERT insertClause?
                     selectClause whereClause? groupByClause? 
                     orderByClause? limitClause?) 
   |
   selectClause
   whereClause?
   groupByClause?  
   orderByClause?
   limitClause?  -> ^(TOK_INSERT ^(TOK_DESTINATION ^(TOK_DIR TOK_TMP_FILE))
                     selectClause whereClause? groupByClause?
                     orderByClause? limitClause?) 
   ;

insertClause
   :
   KW_INSERT destination -> ^(TOK_DESTINATION destination)
   ;

destination
   : KW_TABLE tabName -> ^(tabName)
   ;

//----------------------- Rules for parsing selectClause -----------------------------
// select a,b,c ...
selectClause
    :
    KW_SELECT (KW_ALL | dist=KW_DISTINCT)?
    selectList -> {$dist == NULL}? ^(TOK_SELECT selectList)
               ->                  ^(TOK_SELECTDI selectList)
    |
    KW_MAP trfmClause -> ^(TOK_SELECT ^(TOK_SELEXPR trfmClause KW_MAP) )
    |
    KW_REDUCE trfmClause -> ^(TOK_SELECT ^(TOK_SELEXPR trfmClause KW_REDUCE) )
    ;

selectList
    :
    selectItem ( COMMA  selectItem )* -> selectItem+
    ;

selectItem
    :
    ( selectExpression  (KW_AS Identifier)?) -> ^(TOK_SELEXPR selectExpression Identifier?)
    ;
    
trfmClause
    :
    KW_USING StringLiteral
    (KW_AS (LPAREN aliasList RPAREN | aliasList) )
    (KW_FILTER (LPAREN filterList RPAREN | filterList) )?
    (KW_PARAMS (LPAREN params RPAREN | params) )?
    -> ^(TOK_TRANSFORM StringLiteral aliasList filterList? params?)
    ;
    
selectExpression
    :
    expression | tableAllColumns
    ;

selectExpressionList
    :
    selectExpression (COMMA selectExpression)* -> ^(TOK_EXPLIST selectExpression+)
    ;


//-----------------------------------------------------------------------------------

tableAllColumns
    :
    STAR -> ^(TOK_ALLCOLREF)
    | Identifier DOT STAR -> ^(TOK_ALLCOLREF Identifier)
    ;
    
// table.column
tableColumn
    :
    (tab=Identifier  DOT)? col=Identifier -> ^(TOK_COLREF $tab? $col)
    ;

expressionList
    :
    expression (COMMA expression)* -> ^(TOK_EXPLIST expression+)
    ;

aliasList
    :
    Identifier (COMMA Identifier)* -> ^(TOK_ALIASLIST Identifier+)
    ;

filterList
    :
    filterItem (COMMA filterItem)* -> ^(TOK_FILTERLIST filterItem+)
    ;

filterItem
    :
    Identifier (LPAREN StringLiteral (COMMA StringLiteral)* RPAREN)? -> ^(TOK_FILTERITEM Identifier StringLiteral*)
    ;

params
    :
    StringLiteral -> ^(TOK_PARAMS StringLiteral)
    ; 
   
//----------------------- Rules for parsing fromClause ------------------------------
// from [col1, col2, col3] table1, [col4, col5] table2
fromClause
    :
      KW_FROM joinSource -> ^(TOK_FROM joinSource)
    | KW_FROM fromSource -> ^(TOK_FROM fromSource)
    ;

joinSource    
    :
    fromSource 
    ( joinToken^ fromSource (KW_ON! expression) )
    ;

joinToken
    :
      KW_JOIN                     -> TOK_JOIN
    | KW_LEFT KW_OUTER KW_JOIN    -> TOK_LEFTOUTERJOIN
    | KW_RIGHT KW_OUTER KW_JOIN   -> TOK_RIGHTOUTERJOIN
    | KW_FULL KW_OUTER KW_JOIN    -> TOK_FULLOUTERJOIN
    ;

fromSource
    :
    (tableSource | subQuerySource)
    ;
    
tableSource
    :
    tabname=StringLiteral alias=Identifier -> ^(TOK_TABREF $tabname $alias)
    ;

subQuerySource
    :
    LPAREN queryStatementExpression RPAREN Identifier -> ^(TOK_SUBQUERY queryStatementExpression Identifier)
    ;
        
//----------------------- Rules for parsing whereClause -----------------------------
// where a=b and ...
whereClause
    :
    KW_WHERE searchCondition -> ^(TOK_WHERE searchCondition)
    ;

searchCondition
    :
    expression
    ;

//-----------------------------------------------------------------------------------

// group by a,b
groupByClause
    :
    KW_GROUP KW_BY
    groupByExpression
    ( COMMA groupByExpression )*
    -> ^(TOK_GROUPBY groupByExpression+)
    ;

groupByExpression
    :
    expression
    ;

orderByClause
    :
    KW_ORDER KW_BY
    orderByExpression
    ( COMMA orderByExpression )*
    -> ^(TOK_ORDERBY orderByExpression+)
    ;

orderByExpression
    :
    expression
    (KW_ASC | KW_DESC)?
    ;

limitClause
   :
   KW_LIMIT num=Number -> ^(TOK_LIMIT $num)
   ;

// fun(par1, par2, par3)
function
    :
    functionName
    LPAREN (
          ((dist=KW_DISTINCT)?
           expression
           (COMMA expression)*)?
        )?
    RPAREN -> {$dist == NULL}? ^(TOK_FUNCTION functionName (expression+)?)
                          -> ^(TOK_FUNCTIONDI functionName (expression+)?)

    ;

functionName
    : Identifier 
    ;

constant
    :
    Number
    | StringLiteral
    | booleanValue 
    ;

expression
    :
    precedenceOrExpression
    ;

atomExpression
    :
    KW_NULL -> TOK_NULL
    | constant
    | function
    | tableColumn
    | LPAREN! expression RPAREN!
    ;

precedenceFieldExpression
    :
    atomExpression ((LSQUARE^ expression RSQUARE!) | (DOT^ Identifier))*
    ;

precedenceUnaryOperator
    :
    PLUS | MINUS | TILDE
    ;

precedenceUnaryExpression
    :
      precedenceFieldExpression KW_IS KW_NULL -> ^(TOK_FUNCTION TOK_ISNULL precedenceFieldExpression)
    | precedenceFieldExpression KW_IS KW_NOT KW_NULL -> ^(TOK_FUNCTION TOK_ISNOTNULL precedenceFieldExpression)
    | (precedenceUnaryOperator^)* precedenceFieldExpression
    ;


precedenceBitwiseXorOperator
    :
    BITWISEXOR
    ;

precedenceBitwiseXorExpression
    :
    precedenceUnaryExpression (precedenceBitwiseXorOperator^ precedenceUnaryExpression)*
    ;

	
precedenceStarOperator
    :
    STAR | DIVIDE | MOD
    ;

precedenceStarExpression
    :
    precedenceBitwiseXorExpression (precedenceStarOperator^ precedenceBitwiseXorExpression)*
    ;


precedencePlusOperator
    :
    PLUS | MINUS
    ;

precedencePlusExpression
    :
    precedenceStarExpression (precedencePlusOperator^ precedenceStarExpression)*
    ;


precedenceAmpersandOperator
    :
    AMPERSAND
    ;

precedenceAmpersandExpression
    :
    precedencePlusExpression (precedenceAmpersandOperator^ precedencePlusExpression)*
    ;


precedenceBitwiseOrOperator
    :
    BITWISEOR
    ;

precedenceBitwiseOrExpression
    :
    precedenceAmpersandExpression (precedenceBitwiseOrOperator^ precedenceAmpersandExpression)*
    ;


precedenceEqualOperator
    :
    EQUAL | NOTEQUAL | LESSTHANOREQUALTO | LESSTHAN | GREATERTHANOREQUALTO | GREATERTHAN
    ;

precedenceEqualExpression
    :
    precedenceBitwiseOrExpression (precedenceEqualOperator^ precedenceBitwiseOrExpression)*
    ;


precedenceNotOperator
    :
    KW_NOT
    ;

precedenceNotExpression
    :
    (precedenceNotOperator^)* precedenceEqualExpression
    ;


precedenceAndOperator
    :
    KW_AND
    ;

precedenceAndExpression
    :
    precedenceNotExpression (precedenceAndOperator^ precedenceNotExpression)*
    ;


precedenceOrOperator
    :
    KW_OR
    ;

precedenceOrExpression
    :
    precedenceAndExpression (precedenceOrOperator^ precedenceAndExpression)*
    ;


booleanValue
    :
    KW_TRUE^ | KW_FALSE^
    ;

tabName
   :
   StringLiteral -> ^(TOK_TAB StringLiteral) 
   ;
        
// Keywords
KW_TRUE : 'TRUE';
KW_FALSE : 'FALSE';
KW_ALL : 'ALL';
KW_AND : 'AND';
KW_OR : 'OR';
KW_NOT : 'NOT';
KW_LIKE : 'LIKE';
KW_IF : 'IF';
KW_EXISTS : 'EXISTS';
KW_ASC : 'ASC';
KW_DESC : 'DESC';
KW_ORDER : 'ORDER';
KW_BY : 'BY';
KW_GROUP : 'GROUP';
KW_WHERE : 'WHERE';
KW_FROM : 'FROM';
KW_AS : 'AS';
KW_SELECT : 'SELECT';
KW_DISTINCT : 'DISTINCT';
KW_INSERT : 'INSERT';
KW_OVERWRITE : 'OVERWRITE';
KW_FILTER : 'FILTERS';
KW_PARAMS:  'PARAMS';
KW_OUTER : 'OUTER';
KW_JOIN : 'JOIN';
KW_LEFT : 'LEFT';
KW_RIGHT : 'RIGHT';
KW_FULL : 'FULL';
KW_ON : 'ON';
KW_PARTITION : 'PARTITION';
KW_PARTITIONS : 'PARTITIONS';
KW_TABLE: 'TABLE';
KW_TABLES: 'TABLES';
KW_SHOW: 'SHOW';
KW_MSCK: 'MSCK';
KW_DIRECTORY: 'DIRECTORY';
KW_LOCAL: 'LOCAL';
KW_TRANSFORM : 'TRANSFORM';
KW_USING: 'USING';
KW_CLUSTER: 'CLUSTER';
KW_DISTRIBUTE: 'DISTRIBUTE';
KW_SORT: 'SORT';
KW_UNION: 'UNION';
KW_LOAD: 'LOAD';
KW_DATA: 'DATA';
KW_INPATH: 'INPATH';
KW_IS: 'IS';
KW_NULL: 'NULL';
KW_CREATE: 'CREATE';
KW_EXTERNAL: 'EXTERNAL';
KW_ALTER: 'ALTER';
KW_DESCRIBE: 'DESCRIBE';
KW_DROP: 'DROP';
KW_RENAME: 'RENAME';
KW_TO: 'TO';
KW_COMMENT: 'COMMENT';
KW_BOOLEAN: 'BOOLEAN';
KW_TINYINT: 'TINYINT';
KW_SMALLINT: 'SMALLINT';
KW_INT: 'INT';
KW_BIGINT: 'BIGINT';
KW_FLOAT: 'FLOAT';
KW_DOUBLE: 'DOUBLE';
KW_DATE: 'DATE';
KW_DATETIME: 'DATETIME';
KW_TIMESTAMP: 'TIMESTAMP';
KW_STRING: 'STRING';
KW_ARRAY: 'ARRAY';
KW_MAP: 'MAP';
KW_REDUCE: 'REDUCE';
KW_PARTITIONED: 'PARTITIONED';
KW_CLUSTERED: 'CLUSTERED';
KW_SORTED: 'SORTED';
KW_INTO: 'INTO';
KW_BUCKETS: 'BUCKETS';
KW_ROW: 'ROW';
KW_FORMAT: 'FORMAT';
KW_DELIMITED: 'DELIMITED';
KW_FIELDS: 'FIELDS';
KW_TERMINATED: 'TERMINATED';
KW_COLLECTION: 'COLLECTION';
KW_ITEMS: 'ITEMS';
KW_KEYS: 'KEYS';
KW_KEY_TYPE: '$KEY$';
KW_LINES: 'LINES';
KW_STORED: 'STORED';
KW_SEQUENCEFILE: 'SEQUENCEFILE';
KW_TEXTFILE: 'TEXTFILE';
KW_INPUTFORMAT: 'INPUTFORMAT';
KW_OUTPUTFORMAT: 'OUTPUTFORMAT';
KW_LOCATION: 'LOCATION';
KW_TABLESAMPLE: 'TABLESAMPLE';
KW_BUCKET: 'BUCKET';
KW_OUT: 'OUT';
KW_OF: 'OF';
KW_CAST: 'CAST';
KW_ADD: 'ADD';
KW_REPLACE: 'REPLACE';
KW_COLUMNS: 'COLUMNS';
KW_RLIKE: 'RLIKE';
KW_REGEXP: 'REGEXP';
KW_TEMPORARY: 'TEMPORARY';
KW_FUNCTION: 'FUNCTION';
KW_EXPLAIN: 'EXPLAIN';
KW_EXTENDED: 'EXTENDED';
KW_SERDE: 'SERDE';
KW_WITH: 'WITH';
KW_SERDEPROPERTIES: 'SERDEPROPERTIES';
KW_LIMIT: 'LIMIT';
KW_SET: 'SET';
KW_PROPERTIES: 'TBLPROPERTIES';
KW_VALUE_TYPE: '$VALUE$';
KW_ELEM_TYPE: '$ELEM$';

// Operators

DOT : '.'; // generated as a part of Number rule
COLON : ':' ;
COMMA : ',' ;
SEMICOLON : ';' ;

LPAREN : '(' ;
RPAREN : ')' ;
LSQUARE : '[' ;
RSQUARE : ']' ;

EQUAL : '=';
NOTEQUAL : '<>';
LESSTHANOREQUALTO : '<=';
LESSTHAN : '<';
GREATERTHANOREQUALTO : '>=';
GREATERTHAN : '>';

DIVIDE : '/';
PLUS : '+';
MINUS : '-';
STAR : '*';
MOD : '%';

AMPERSAND : '&';
TILDE : '~';
BITWISEOR : '|';
BITWISEXOR : '^';

// LITERALS
fragment
Letter
    : 'a'..'z' | 'A'..'Z'
    ;

fragment
HexDigit
    : 'a'..'f' | 'A'..'F' 
    ;

fragment
Digit
    :
    '0'..'9'
    ;

fragment
Exponent
    :
    'e' ( PLUS|MINUS )? (Digit)+
    ;

StringLiteral
    :
    ( '\'' (~'\'')* '\'' | '\"' (~'\"')* '\"' )+
    ;

Number
    :
    (Digit)+ ( DOT (Digit)* (Exponent)? | Exponent)?
    ;

Identifier
    :
    (Letter | Digit) (Letter | Digit | '_')*
    | '`' (Letter | Digit) (Letter | Digit)* '`'
    ;

WS  :  (' '|'\r'|'\t'|'\n') {$channel=HIDDEN;}
    ;

COMMENT
  : '--' (~('\n'|'\r'))*
    { $channel=HIDDEN; }
  ;
