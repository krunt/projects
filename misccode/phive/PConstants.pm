package PConstants;

use strict;
use warnings;

use constant {
TOK_INSERT=>4,
TOK_QUERY=>5,
TOK_SELECT=>6,
TOK_SELECTDI=>7,
TOK_SELEXPR=>8,
TOK_FROM=>9,
TOK_TAB=>10,
TOK_PARTSPEC=>11,
TOK_PARTVAL=>12,
TOK_DIR=>13,
TOK_LOCAL_DIR=>14,
TOK_TABREF=>15,
TOK_SUBQUERY=>16,
TOK_DESTINATION=>17,
TOK_ALLCOLREF=>18,
TOK_COLREF=>19,
TOK_FUNCTION=>20,
TOK_FUNCTIONDI=>21,
TOK_WHERE=>22,
TOK_OP_EQ=>23,
TOK_OP_NE=>24,
TOK_OP_LE=>25,
TOK_OP_LT=>26,
TOK_OP_GE=>27,
TOK_OP_GT=>28,
TOK_OP_DIV=>29,
TOK_OP_ADD=>30,
TOK_OP_SUB=>31,
TOK_OP_MUL=>32,
TOK_OP_MOD=>33,
TOK_OP_BITAND=>34,
TOK_OP_BITNOT=>35,
TOK_OP_BITOR=>36,
TOK_OP_BITXOR=>37,
TOK_OP_AND=>38,
TOK_OP_OR=>39,
TOK_OP_NOT=>40,
TOK_OP_LIKE=>41,
TOK_TRUE=>42,
TOK_FALSE=>43,
TOK_TRANSFORM=>44,
TOK_EXPLIST=>45,
TOK_ALIASLIST=>46,
TOK_GROUPBY=>47,
TOK_ORDERBY=>48,
TOK_CLUSTERBY=>49,
TOK_DISTRIBUTEBY=>50,
TOK_SORTBY=>51,
TOK_UNION=>52,
TOK_JOIN=>53,
TOK_LEFTOUTERJOIN=>54,
TOK_RIGHTOUTERJOIN=>55,
TOK_FULLOUTERJOIN=>56,
TOK_LOAD=>57,
TOK_NULL=>58,
TOK_ISNULL=>59,
TOK_ISNOTNULL=>60,
TOK_TINYINT=>61,
TOK_SMALLINT=>62,
TOK_INT=>63,
TOK_BIGINT=>64,
TOK_BOOLEAN=>65,
TOK_FLOAT=>66,
TOK_DOUBLE=>67,
TOK_DATE=>68,
TOK_DATETIME=>69,
TOK_TIMESTAMP=>70,
TOK_STRING=>71,
TOK_LIST=>72,
TOK_MAP=>73,
TOK_CREATETABLE=>74,
TOK_DESCTABLE=>75,
TOK_ALTERTABLE_RENAME=>76,
TOK_ALTERTABLE_ADDCOLS=>77,
TOK_ALTERTABLE_REPLACECOLS=>78,
TOK_ALTERTABLE_ADDPARTS=>79,
TOK_ALTERTABLE_DROPPARTS=>80,
TOK_ALTERTABLE_SERDEPROPERTIES=>81,
TOK_ALTERTABLE_SERIALIZER=>82,
TOK_ALTERTABLE_PROPERTIES=>83,
TOK_MSCK=>84,
TOK_SHOWTABLES=>85,
TOK_SHOWPARTITIONS=>86,
TOK_CREATEEXTTABLE=>87,
TOK_DROPTABLE=>88,
TOK_TABCOLLIST=>89,
TOK_TABCOL=>90,
TOK_TABLECOMMENT=>91,
TOK_TABLEPARTCOLS=>92,
TOK_TABLEBUCKETS=>93,
TOK_TABLEROWFORMAT=>94,
TOK_TABLEROWFORMATFIELD=>95,
TOK_TABLEROWFORMATCOLLITEMS=>96,
TOK_TABLEROWFORMATMAPKEYS=>97,
TOK_TABLEROWFORMATLINES=>98,
TOK_TBLSEQUENCEFILE=>99,
TOK_TBLTEXTFILE=>100,
TOK_TABLEFILEFORMAT=>101,
TOK_TABCOLNAME=>102,
TOK_TABLELOCATION=>103,
TOK_PARTITIONLOCATION=>104,
TOK_TABLESAMPLE=>105,
TOK_TMP_FILE=>106,
TOK_TABSORTCOLNAMEASC=>107,
TOK_TABSORTCOLNAMEDESC=>108,
TOK_CHARSETLITERAL=>109,
TOK_CREATEFUNCTION=>110,
TOK_EXPLAIN=>111,
TOK_TABLESERIALIZER=>112,
TOK_TABLEPROPERTIES=>113,
TOK_TABLEPROPLIST=>114,
TOK_TABTYPE=>115,
TOK_LIMIT=>116,
TOK_TABLEPROPERTY=>117,
TOK_IFNOTEXISTS=>118,
TOK_FILTERLIST=>119,
TOK_FILTERITEM=>120,
TOK_PARAMS=>121,
KW_INSERT=>122,
KW_TABLE=>123,
KW_SELECT=>124,
KW_ALL=>125,
KW_DISTINCT=>126,
KW_MAP=>127,
KW_REDUCE=>128,
COMMA=>129,
KW_AS=>130,
Identifier=>131,
KW_USING=>132,
StringLiteral=>133,
LPAREN=>134,
RPAREN=>135,
KW_FILTER=>136,
KW_PARAMS=>137,
STAR=>138,
DOT=>139,
KW_FROM=>140,
KW_ON=>141,
KW_JOIN=>142,
KW_LEFT=>143,
KW_OUTER=>144,
KW_RIGHT=>145,
KW_FULL=>146,
KW_WHERE=>147,
KW_GROUP=>148,
KW_BY=>149,
Number=>150,
KW_NULL=>151,
LSQUARE=>152,
RSQUARE=>153,
PLUS=>154,
MINUS=>155,
TILDE=>156,
KW_IS=>157,
KW_NOT=>158,
BITWISEXOR=>159,
DIVIDE=>160,
MOD=>161,
AMPERSAND=>162,
BITWISEOR=>163,
EQUAL=>164,
NOTEQUAL=>165,
LESSTHANOREQUALTO=>166,
LESSTHAN=>167,
GREATERTHANOREQUALTO=>168,
GREATERTHAN=>169,
KW_AND=>170,
KW_OR=>171,
KW_TRUE=>172,
KW_FALSE=>173,
KW_LIKE=>174,
KW_IF=>175,
KW_EXISTS=>176,
KW_ASC=>177,
KW_DESC=>178,
KW_ORDER=>179,
KW_OVERWRITE=>180,
KW_PARTITION=>181,
KW_PARTITIONS=>182,
KW_TABLES=>183,
KW_SHOW=>184,
KW_MSCK=>185,
KW_DIRECTORY=>186,
KW_LOCAL=>187,
KW_TRANSFORM=>188,
KW_CLUSTER=>189,
KW_DISTRIBUTE=>190,
KW_SORT=>191,
KW_UNION=>192,
KW_LOAD=>193,
KW_DATA=>194,
KW_INPATH=>195,
KW_CREATE=>196,
KW_EXTERNAL=>197,
KW_ALTER=>198,
KW_DESCRIBE=>199,
KW_DROP=>200,
KW_RENAME=>201,
KW_TO=>202,
KW_COMMENT=>203,
KW_BOOLEAN=>204,
KW_TINYINT=>205,
KW_SMALLINT=>206,
KW_INT=>207,
KW_BIGINT=>208,
KW_FLOAT=>209,
KW_DOUBLE=>210,
KW_DATE=>211,
KW_DATETIME=>212,
KW_TIMESTAMP=>213,
KW_STRING=>214,
KW_ARRAY=>215,
KW_PARTITIONED=>216,
KW_CLUSTERED=>217,
KW_SORTED=>218,
KW_INTO=>219,
KW_BUCKETS=>220,
KW_ROW=>221,
KW_FORMAT=>222,
KW_DELIMITED=>223,
KW_FIELDS=>224,
KW_TERMINATED=>225,
KW_COLLECTION=>226,
KW_ITEMS=>227,
KW_KEYS=>228,
KW_KEY_TYPE=>229,
KW_LINES=>230,
KW_STORED=>231,
KW_SEQUENCEFILE=>232,
KW_TEXTFILE=>233,
KW_INPUTFORMAT=>234,
KW_OUTPUTFORMAT=>235,
KW_LOCATION=>236,
KW_TABLESAMPLE=>237,
KW_BUCKET=>238,
KW_OUT=>239,
KW_OF=>240,
KW_CAST=>241,
KW_ADD=>242,
KW_REPLACE=>243,
KW_COLUMNS=>244,
KW_RLIKE=>245,
KW_REGEXP=>246,
KW_TEMPORARY=>247,
KW_FUNCTION=>248,
KW_EXPLAIN=>249,
KW_EXTENDED=>250,
KW_SERDE=>251,
KW_WITH=>252,
KW_SERDEPROPERTIES=>253,
KW_LIMIT=>254,
KW_SET=>255,
KW_PROPERTIES=>256,
KW_VALUE_TYPE=>257,
KW_ELEM_TYPE=>258,
COLON=>259,
SEMICOLON=>260,
Letter=>261,
HexDigit=>262,
Digit=>263,
Exponent=>264,
WS=>265,
COMMENT=>266,

    MAIN_SELECT => 'MAIN_SELECT',
};

1;
