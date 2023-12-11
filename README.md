## MySqlExtBind - MySql bind extension

Wrapper for the MySQL function `mysql_stmt_bind_named_param()` in order to meet the _expected_ named parameters behaviour. This means, the `:bindName` syntax can be used in the SQL statement.

This [post](https://stackoverflow.com/questions/77626696/understanding-mysql-stmt-bind-named-param-mysql-c-api) in SO was the start. The answer is more or less very clear regarding the _understanding_ what's gone wrong with the MySQL developers in order to implement such a feature and calling it _named_. The [MySQL documentation](https://dev.mysql.com/doc/c-api/8.2/en/mysql-stmt-bind-named-param.html) and the example there is one of the worst I have seen.

The motivation to write this small extension was the wish to use named bind variables in the C API like in this example:

```plaintext
INSERT INTO foo SET
   bar_int    = :barInt, 
   bar_date   = :barDate, 
   bar_vchar  = :barVchar,
   bar_double = :barDouble;
```

_The extension should meet this requirements:_

1.  The order of the bind variables and the function calls to _bind_ their values must be independent, so it's more convenient and less buggy.
2.  No limitations - only the system sets the limits.
3.  Almost any delimiters can be used. For example `:{barInt}`, `[^barInt$]` and other _endless_ combinations.
4.  Light weighted code.
5.  The extension prevents any undefined situation/behaviour like:

> The delimiters are not correct and cannot be read by C++ regex.
> 
> The bind variables in the SQL command and latter usage do not match. This means, some bind variables have been _forgotten_ or was not _introduced_ in the SQL command \[this is mostly a typo\].

#### **Note:**

You cannot used this extension if you use the original MySQL “named” functionality as the extension removes all attributes.

#### Known issues:

The extension does not implement the MySQL parser at all. It applies a regular expression to the provided MySQL command in order to extract the bind variables. Usually, this shouldn't be a problem. It breaks the functionality when you use a string which may contain a word surrounded by the delimiters you use. This string, for example, breaks the execution when using the default delimiters: “`:some_plaintext_which_is_not_a_bind_variable`”. 

If you run into this issue you have to use different delimiters which should be more complex.

---

### Installation

The project consists of 2 files:

1.  `MySqlExtBind.cpp`
2.  `MySqlExtBind.h`

Embed them in your project and make sure you include `MySqlExtBind.h` in the source where you use the `MySqlExtBind` functions.

---

### Compiling

The minimum requirements is `C++17`. While the development the GNU C++ compiler `g++-13` has been used with the `pedantic` switch, so the source should compile with any ANSI C++ compiler. Use this compiler options:

``-O3 -std=c++17 -c -pedantic -pedantic-errors -Wall -Werror -Wextra -Wshadow -Wformat-signedness -m64 -fPIC `mysql_config --include` ``

You can change the architecture for your needs.

The extensions needs at least MySQL 8.0.

---

### Examples

#### Using the default delimiters

*   Instantiate the variable providing a pointer to an existing and initialised `MYSQL_STMT *`:

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
  ... Set the <dateTime> members ...

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

*   Set the new delimiters - see section `Function reference` for a detailed description and remarks:

```cpp
FaF::MySqlExtBind::setDelimiters( ":\\{", "\\}" );
```

*   Use this delimiters in your MySQL command to mark the bind variables:

```cpp
const char * insertCommand { "INSERT INTO foo SET bar_int = :{barInt}, bar_char = :{barChar}, bar_date = :{barDate}" };
```

The rest remains the same like in the first example with the default delimiters.

---

### Function reference

The extension is embedded in the `FaF` namespace. 

*   **Initialise the class with the constructor.**

```cpp
MySqlExtBind( MYSQL_STMT * mysqlStatementStruct, const std::string mysqlCommand );
```

Provide the already initialised `MYSQL_STMT *` variable \[`mysqlStatementStruct`\] and the MySQL command \[`mysqlCommand`\].

_Example:_

```cpp
FaF::MySqlExtBind fafExtBind( preparedStatement, mysqlCommand );
```

The extension tries to parse `mysqlCommand` and collects all bind variables. It throws these exceptions in following cases - see also the below section `Exceptions`:

> *   The delimiters - see function `setDelimiters(..)` - cannot be understood by the C++ `regex` implementation.
> *   No bind variables have been found in the MySQL command. At least 1 bind variable must be used. If you call the constructor with a MySQL command, which has bind variables once and then again not, then you have to adjust your logic so that the extension is only called with a MySQL command if it has at least one bind variable.

*   **Execute the original MySQL** `mysql_stmt_prepare()` **function.**

```cpp
auto prepareStatement() -> decltype( mysql_stmt_prepare( nullptr, nullptr, 0 ) );
```

It's clear that the provided MySQL command never can be understood by the MySQL Server. Therefore the extension's constructor is modifying the provided MySQL command in order it can be prepared and executed. This implies that you have to call the extension's prepare wrapper which replaces the original MySQL command by the adjusted command.

_The modification is quite straightforward_: All bind variables are replaced by the primitive MySQL `?` placeholder.

_Example:_

```cpp
auto mysqlErrorCode = fafExtBind.prepareStatement();
```

The returned value corresponds to the original MySQL `mysql_stmt_prepare()` return value and type. See the original MySQL documentation for detailed information.

*   **Assign the bind value to a bind variable.**

Use this function to bind a value to a bind variable introduced previously in the constructor. 2 overloads exist for this function. _Bind_ the provided `MYSQL_BIND` structure in `originalMysqlBindItem` to the `bindVariable` \[note, that you do **NOT** use the delimiters here!\].

> Using the `MYSQL_BIND` structure.

```cpp
auto assignBindData( const std::string bindVariable, const MYSQL_BIND & originalMysqlBindItem ) -> void;
```

_Example:_

```cpp
MYSQL_BIND mysqlBindStructure;     // Note: the usual array definition is NOT used in this case as we have only 1 item.
   ... Set the members in mysqlBindStructure ...
FaF::assignBindData( "barInt", mysqlBindStructure );
```

> Providing the most important `MYSQL_BIND` values directly to the functions without the `MYSQL_BIND` structure.

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

The 4 `MYSQL_BIND` members can be set directly as function parameters. If you need more members so open an issue and I'll have a look. It throws an exception if the bind variable provided in `bindVariable` hasn't been introduced in the MySQL command used for the constructor.

*   **Run the original MySQL** `mysql_stmt_bind_named_param()` **function.**

```cpp
auto executeBind() -> decltype( mysql_stmt_bind_named_param( nullptr, nullptr, 0, nullptr ) );
```

Runs the original MySQL `mysql_stmt_bind_named_param()` function with the adapted arrays/variables.

_Example:_

```cpp
mysqlErrorCode = fafExtBind.executeBind();
```

The returned value corresponds to the original MySQL `mysql_stmt_bind_named_param()` return value and type. See the original MySQL documentation for detailed information.

*   **Set new Regex pattern for the delimiters.**

Set the left and right Regex patterns for the delimiters used in the MySQL command for the mark the bind fields for the Regex parser.

```cpp
static auto setDelimiters( const std::string leftDelimiter = ":", const std::string rightDelimiter = "" ) -> void;
```

The delimiters for left and right can be set individually. Keep in mind to escape the patterns. However, the extension throws an exception if the pattern cannot be recognised. Test any new delimiters pattern in order to assure the exception is not thrown in a production environment. Usually, there is no need to modify the delimiters and the default delimiter `“:”` can be used.

_Examples:_

```cpp
FaF::MySqlExtBind::setDelimiters( ":\\{", "\\]" );
```

>    … Format of the bind variables in the MySQL command …

```cpp
INSERT INTO foo SET bar_int = :{barInt}
```

---

```cpp
FaF::MySqlExtBind::setDelimiters( "\\[^", "\\$]" );
```

>    … Format of the bind variables in the MySQL command …

```cpp
INSERT INTO foo SET bar_int = [^barInt$]
```

---

```cpp
FaF::MySqlExtBind::setDelimiters( R"(\/)", R"(\\)" );
```

>    … Format of the bind variables in the MySQL command …

```cpp
R"~(INSERT INTO foo SET bar_int = /barInt\ )~"
```

---

Almost every combination can be used, it depends only on the C++ Regex parser. Also note that it's a good advise to use the C++ `raw string literals` with the `R` prefix. However, I would try to keep it simple and not to confuse yourself unnecessarily. You have to consider 2 points:

1.  How to escape the characters for the C++ compiler and then,
2.  what _remains_ for the C++ Regex parser while the program execution.

This problematic can be seen well in the last example.

---

### Exceptions

The extension throws an exception if an abnormal situation is detected. Below is the explanation for the numbered exceptions:

#### Exception #1:

> Exception #1: No bind variable has been found with the provided delimiters.  
>   1) Check the delimiters. Have the characters been correctly escaped?  
>   2) Are the bind variables between the delimiters?  
>   3) At least one bind variable must be used in the SQL command.

This exception is thrown in the constructor. It gives already exhausting hints what the problem can be. Try to simplify the patterns, check if the bind variables are correctly surrounded by the delimiters and if the MySQL command at least does have 1 bind variable.

#### Exception #2:

> Exception #2. Regex failed. Check the delimiters and if characters have been correctly escaped.

The C++ compiler could process the pattern string but the Regex parser doesn't understand it. An example for such a string:

```cpp
FaF::MySqlExtBind::setDelimiters( R"(()", R"(\\)" );
```

In this case the opening `(` must be escaped even the C++ compiler understands it.

#### Exception #3:

> Exception #3: Bind variable \[XYZ\] not found. Mostly a typo or an incorrect delimiters.

The name of the bind variable provided for the `assignBindVariable()` function hasn't been found in the MySQL command. It's mostly a typo, wrong delimiters or a problem in the code logic \[missing bind variable in the MySQL command\]. An example for a typo, instead of `barInt` the non existing name `barInz` was provided:

```cpp
fafExtBind.assignBindData( "barInz", MYSQL_TYPE_LONG, static_cast<void *>(&int_bar) );
```

#### Exception #4:

> Exception #4: For the bind variables in the below list `assignBindData()` has NOT been called.
> 
> \[barInt\]

This exception is thrown in the `executeBind()` function which detects that not all bind variables have been set. In order to minimise bugs and keep the logic clear, for each bind variables provided in the MySQL command the function `assignBindData()` must be called.