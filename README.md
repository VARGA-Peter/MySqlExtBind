## MySqlExtBind - extend the MySql bind functionality

Wrapper for the MySQL function mysql\_stmt\_bind\_named\_param() for the expected named parameters behaviour. This means, the `:bindName` syntax can be used in the SQL statement.

This [post](https://stackoverflow.com/questions/77626696/understanding-mysql-stmt-bind-named-param-mysql-c-api) in SO was the start. The answer is more or less very clear regarding the _understanding_ what's gone wrong with the MySQL developers in order to implement such a feature and calling it _named_. The [MySQL documentation](https://dev.mysql.com/doc/c-api/8.2/en/mysql-stmt-bind-named-param.html) and the example there is one of the worst I have seen.

The motivation to write this small extension was the wish to use named bind variables in the C API. For example:

```plaintext
INSERT INTO foo SET
   bar_int    = :barInt, 
   bar_date   = :barDate, 
   bar_vchar  = :barVchar,
   bar_double = :barDouble;
```

_These requirements should be met:_

1.  The order of the bind variables and the function calls to _bind_ their values must be independent, so it's more convenient and less buggy.
2.  No limitations - only the system sets the limits.
3.  Almost any delimiters can be used. For example `:{barInt}`, `[^barInt$]` and other _endless_ combinations.
4.  Light weighted code.
5.  The extension checks any undefined situation like:

> The delimiters are correct and can be read by C++ regex.
> 
> The bind variables in the SQL command and later usage do match. This means, no one was _forgotten_ or a bind variable was not _introduced_ in the SQL command \[this is mostly a typo\].

#### **Note:**

You cannot used this extension if you use the MySQL “named” functionality as the extension removes all attributes.

---

### Installation

The project consists of 2 files:

1.  `MySqlExtBind.cpp`
2.  `MySqlExtBind.h`

Embed them in your project and make sure you include `MySqlExtBind.h` in the source where you use the `MySqlExtBind` functions.

---

### Compiling

The minimum requirements is `C++17`. While the development `g++-13` has been used with the `pedantic` switch, so the source should compile with each ANSI C++ compiler. Use this compiler options:

`-O3 -std=c++17 -c -pedantic -pedantic-errors -Wall -Werror -Wextra -Wshadow -Wformat-signedness -m64 -fPIC`

You can change the architecture for your needs.

The extensions needs at least MySQL 8.0.

---

### Examples

#### Using the default delimiters

*   Instantiate the variable providing a pointer to an existing and initialised MYSQL\_STMT \*:

```cpp
const char * insertCommand { "INSERT INTO foo SET bar_int = :barInt, bar_char = :barChar, bar_date = :barDate " };
MYSQL_STMT * preparedInsertStatement = mysql_stmt_init( mysqlConnection );

FaF::MySqlExtBind fafExtBind( preparedInsertStatement, insertCommand );
```

*   Execute the original MySQL `mysql_stmt_prepare()` function. See the below section `Function reference` for further explanations.

`auto mysqlErrorCode = fafExtBind.prepareStatement();`

*   Set for each bind variable its value - note that the order of `assignBindData()` calls does **NOT** match the order in the original `INSERT` command:

```cpp
int  intBar     { 2804 };
char intChar [] { "Some-Text" };
size_t intCharLength = strlen(intChar);
MYSQL_TIME dateTime {};
  ... Set for dateTime the members

fafExtBind.assignBindData( "barChar", MYSQL_TYPE_STRING,   intChar, &intCharLength );
fafExtBind.assignBindData( "barInt",  MYSQL_TYPE_LONG,     static_cast<void *>(&intBar) );
fafExtBind.assignBindData( "barDate", MYSQL_TYPE_DATETIME, &dateTime );
```

*   Run the original MySQL `mysql_stmt_bind_named_param()` function with the correctly prepared arrays.

```cpp
mysqlErrorCode = fafExtBind.executeBind();
```

The row has been inserted into the table with the according values.

---

#### Using other delimiters

*   Set the new delimiters - see section Functions for a detailed description and remarks:

```cpp
FaF::MySqlExtBind::setDelimiters( ":\\{", "\\}" );
```

*   Instantiate the variable providing a pointer to an existing and initialised MYSQL\_STMT \*:

```cpp
const char * insertCommand { "INSERT INTO foo SET bar_int = :{barInt}, bar_char = :{barChar}, bar_date = :{barDate}" };
FaF::MySqlExtBind fafExtBind( preparedInsertStatement, insertCommand );
```

The rest remains the same like in the first example with the default delimiters.

---

### Function reference

The extension is embedded in the `FaF` namespace.

*   **Initialise the class with the constructor.**

```cpp
MySqlExtBind( MYSQL_STMT * mysqlStatementStruct, const std::string mysqlCommand )
```

Provide the already initialised `MYSQL_STMT *` variable \[`mysqlStatementStruct`\] and the MySQL command \[`mysqlCommand`\].

_Example:_

```cpp
FaF::MySqlExtBind fafExtBind( preparedStatement, mysqlCommand );
```

The extension tries to parse `mysqlCommand` and collect all bind variables. It throws these exceptions in following cases - see also the below section `Exceptions`:

> *   The delimiters - see function `setDelimiters(..)` - cannot be understood by the C++ `regex` implementation.
> *   No bind variables have been found in the MySQL command. At least 1 bind variable must be used. If the MySQL command, used with this instance variable, does not have always a bind variable then you have to implement it in your code logic that you don't use the extension.

*   **Execute the original MySQL** `mysql_stmt_prepare()` **function.**

```cpp
auto prepareStatement() -> decltype( mysql_stmt_prepare( nullptr, nullptr, 0 ) );
```

It's clear that the provided MySQL command never can be understood by the MySQL Server. Therefore the extension's constructor is modifying the provided MySQL command in order it can be prepared and executed.

_The modification is quite straightforward_: All bind variables are replaced by the primitive MySQL `?` placeholder.

This implicits that you have to call the extension's prepare wrapper which replaces the original MySQL command by the adjusted command.

_Example:_

```cpp
auto mysqlErrorCode = fafExtBind.prepareStatement();
```

The returned value corresponds to the original MySQL `mysql_stmt_prepare()` return value. See the original MySQL documentation for detailed information.

*   **Assign the bind value to a bind variable.**

Use this function to bind a value to a bind variable introduced previously in the constructor. There are 2 overloads of this function:

> Use the `MYSQL_BIND` structure.

```cpp
auto assignBindData( const std::string bindVariable, const MYSQL_BIND & originalMysqlBindItem ) -> void;
```

_Bind_ the provided `MYSQL_BIND` structure to the `bindVariable` \[note, that you do **NOT** use the delimiters here!\].

_Example:_

```cpp
MYSQL_BIND mysqlBindStructure;     // Note: the usual array definition is NOT used in this case as we have only 1 item.
   ... Set the members in mysqlBindStructure ...
FaF::assignBindData( "barInt", mysqlBindStructure );
```

> Provide the most important `MYSQL_BIND` values directly to the functions without the `MYSQL_BIND` structure.

```cpp
auto assignBindData(
                   const std::string bindVariable,
                   // m_finalMysqlBindArray is only a placeholder so we can access the MYSQL_BIND's members.
                   decltype( m_finalMysqlBindArray->buffer_type ),
                   decltype( m_finalMysqlBindArray->buffer      ),
                   decltype( m_finalMysqlBindArray->length      ) length  = nullptr,
                   decltype( m_finalMysqlBindArray->is_null     ) is_null = nullptr
           ) -> void;
```

The 4 `MYSQL_BIND` members can be set directly in the function call. If you need more so open an issue and I'll have a look. It throws an exception if the bind variable provided in `bindVariable` hasn't been introduced in the MySQL command used for the constructor.

*   **Run the original MySQL** `**mysql_stmt_bind_named_param()**` **function.**

```cpp
auto executeBind() -> decltype( mysql_stmt_bind_named_param( nullptr, nullptr, 0, nullptr ) );
```

Runs the original MySQL `mysql_stmt_bind_named_param()` function with the adapted arrays/variables.

Example:

```cpp
mysqlErrorCode = fafExtBind.executeBind();
```

The returned value corresponds to the original MySQL `mysql_stmt_bind_named_param()` return value. See the original MySQL documentation for detailed information.