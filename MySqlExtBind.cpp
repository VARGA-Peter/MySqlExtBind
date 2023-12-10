/**
 * FaFMySqlStmtBindNamedParam.cpp
 *
 * Extending the mysql_stmt_bind_named_param() function with real named bind variables using the :bindVariable syntax.
 * Check README.md for more information.
 *
 * Written by Peter VARGA
 * Created 2023-12-09
 *
 * Version 1.00
 *
 */

#include "MySqlExtBind.h"

bool        FaF::MySqlExtBind::m_regexHasBeenChecked {};
std::string FaF::MySqlExtBind::m_leftDelimiter       { ":" };
std::string FaF::MySqlExtBind::m_rightDelimiter      {};

namespace FaF
{

    /**
     * Parse the SQL command and assure the delimiters can be processed by regex.
     *
     * @param mysqlStatementStruct
     * @param mysqlCommand
     */
    MySqlExtBind::MySqlExtBind( MYSQL_STMT * mysqlStatementStruct, const std::string mysqlCommand )
    :
        m_mysqlStatementStruct( mysqlStatementStruct ),
        m_mysqlCommand        ( mysqlCommand         ),
        m_adjustedMysqlCommand( mysqlCommand         )
    {

        parseMysqlCommand();

    }

    /**
     * Sets new left and right delimiters, so the bind variable can be recognised.
     * For example:
     *  left:   :\\{
     *  right:  \\}
     * Then the bind variables will be recognised when used like this: :{fooBar}
     *
     * @param leftDelimiter
     * @param rightDelimiter
     * @return
     */
    auto MySqlExtBind::setDelimiters( const std::string leftDelimiter, const std::string rightDelimiter ) -> void
    {

        MySqlExtBind::m_leftDelimiter  = leftDelimiter;
        MySqlExtBind::m_rightDelimiter = rightDelimiter;

    }

    /**
     * Looks for bind variables according to the current delimiters and prepares the internal array,
     * so later each bind variable can set easily.
     *
     * @param runRegexTest
     * @return
     */
    auto MySqlExtBind::parseMysqlCommand() -> void
    {

        // Needed only for the regex test.
        bool anyMatchFound {};

        const std::string resolvedPattern {                                 // @suppress("Invalid arguments")
            MySqlExtBind::m_leftDelimiter +
            R"~((\w+))~" +
            MySqlExtBind::m_rightDelimiter
                                          };

        try {

            const std::regex regexPattern( resolvedPattern );

            std::sregex_iterator currentRegexMatch( m_mysqlCommand.begin(), m_mysqlCommand.end(), regexPattern );
            std::sregex_iterator endMarker;

            // The position is needed in order to construct the MYSQL_BIND array in the correct order.
            m_bindVariablesCount = 0;

            while( currentRegexMatch != endMarker ) {

                const std::string fullMatch { (*currentRegexMatch) [0] };        // @suppress("Invalid arguments")
                const std::string onlyMatch { (*currentRegexMatch) [1] };        // @suppress("Invalid arguments")

                // Add the found bind variable to the container. The MYSQL_BIND item is empty - it will be set in assignBindData().
                m_bindNamesContainer.insert( MapContainer::value_type( onlyMatch, { m_bindVariablesCount++ } ) );

                // Replace the bind placeholder by <?>.
                m_adjustedMysqlCommand.replace( m_adjustedMysqlCommand.find(fullMatch), fullMatch.length(), "?" );

                // Any match has been found, set the flag for the regex test.
                anyMatchFound = true;

                currentRegexMatch++;

            }

            if ( false == anyMatchFound ) {

                // The pattern seems not to work. The test pattern hasn't been found. Throw an exception.
                std::cerr
                    << "No bind variable has been found with the provided delimiters. "         << std::endl
                    << " 1) Check the delimiters. Have the characters been correctly escaped?"  << std::endl
                    << " 2) Are the bind variables between the delimiters?"                     << std::endl
                    << " 3) At least one bind variable must be used in the SQL command."        << std::endl
                    << std::endl;
                throw FaF::Exception();

            }

        } catch ( std::regex_error const & ) {

            // A fatal regex error - the delimiters are nonsense.
            std::cerr << "Regex exception raised. Check the delimiters and if characters have been correctly escaped." << std::endl;
            throw FaF::Exception();

        }

    }

    /**
     * Overload for const std::string
     * Note: If <bindName> is not found in the map, an exception is thrown!
     *
     * @param bindName
     * @param originalMysqlBindItem
     */
    auto MySqlExtBind::assignBindData( const std::string bindVariable, const MYSQL_BIND & originalMysqlBindItem ) -> void
    {

        // Copy the structure item and throw an exception if <bindVariable> is not found.
        copyBindStructure( bindVariable, originalMysqlBindItem );

    }

    /**
     * More convenient way to add a bind variable, without instantiating MYSQL_BIND and then set each of the values.
     * If you need more members from MYSQL_BIND, open an issue in GitHub.
     * An exception is thrown if <bindVariable> cannot be found. Mostly a typo...
     *
     * @param
     * @param
     * @param
     * @param length
     * @param is_null
     * @return
     */
    auto MySqlExtBind::assignBindData(
            const std::string bindVariable,
            // m_finalMysqlBindArray is only a placeholder so we can access the MYSQL_BIND's members.
            decltype( m_finalMysqlBindArray->buffer_type ) buffer_type,
            decltype( m_finalMysqlBindArray->buffer      ) buffer,
            decltype( m_finalMysqlBindArray->length      ) length,
            decltype( m_finalMysqlBindArray->is_null     ) is_null
    ) -> void
    {

        MYSQL_BIND mysqlBindItem {};

        mysqlBindItem.buffer_type = buffer_type;
        mysqlBindItem.buffer      = buffer;
        mysqlBindItem.length      = length;
        mysqlBindItem.is_null     = is_null;

        // Copy the structure item and throw an exception if <bindVariable> is not found.
        copyBindStructure( bindVariable, mysqlBindItem );

    }

    /**
     * Copies the provided entry into the already existing and internal MYSQL_BIND structure because we know now the position
     * in the SQL command bin list.
     *
     * @param bindVariable
     * @param sourceBindStructure
     * @return
     */
    auto MySqlExtBind::copyBindStructure( const std::string bindVariable, const MYSQL_BIND & sourceBindStructure ) -> void
    {

        try {

            auto & currenContainerItem = m_bindNamesContainer.at(bindVariable);

            // Mark the item that the value has been set.
            currenContainerItem.assignBindData = true;
            // Copy the MYSQL_BIND data.
            currenContainerItem.mysqlBindItem  = sourceBindStructure;

        } catch ( std::out_of_range const & ) {

            using namespace std::string_literals;

            std::cerr
                << "Bind variable ["s + bindVariable.c_str() + "] not found. Mostly a typo or an incorrect delimiters."s
                << std::endl;
            throw FaF::Exception();

        }

    }

    /**
     * Calls mysql_stmt_prepare() with the adjusted SQL command. The return type is deducted from mysql_stmt_prepare().
     *
     * @return
     */
    auto MySqlExtBind::prepareStatement() -> decltype( mysql_stmt_prepare( nullptr, nullptr, 0 ) )
    {

        return mysql_stmt_prepare( m_mysqlStatementStruct, m_adjustedMysqlCommand.c_str(), strlen(m_adjustedMysqlCommand.c_str()) );

    }

    /**
     * Calls mysql_stmt_bind_named_param() and with the provided bind values.
     * The return type is deducted from mysql_stmt_bind_named_param().
     *
     * @return
     */
    auto MySqlExtBind::executeBind() -> decltype( mysql_stmt_bind_named_param( nullptr, nullptr, 0, nullptr ) )
    {

        const char ** mysqlNamed    = new const char * [m_bindVariablesCount]();
        MYSQL_BIND * mysqlBindArray = new MYSQL_BIND   [m_bindVariablesCount]();

        for ( auto const & [bindVariable, bindItem] : m_bindNamesContainer ) {

            /**
             * Just copy to the index which corresponds the position of the bind variable in the SQL command
             * the earlier provided MYSQL_BIND item.
             */
            mysqlBindArray [bindItem.bindNamePosition] = bindItem.mysqlBindItem;

        }

        // Not all bind variables have been set using assignBindData().
        std::string bindVariablesList {};
        for ( auto & [bindVariable, bindItem] : m_bindNamesContainer ) {

            if ( false == bindItem.assignBindData ) {

                bindVariablesList += ( 0 == bindVariablesList.length() ? "" : ", " ) + bindVariable;

            }

            // Reset it for the next call in the same instance.
            bindItem.assignBindData = false;

        }

        if ( 0 != bindVariablesList.length() ) {

            std::cerr
                << "For the bind variables in the below list assignBindData() has NOT been called." << std::endl
                << "[" + bindVariablesList + "]"
                << std::endl;
            throw FaF::Exception();

        }

        // Run the original MySql bind function with the correctly prepared arrays.
        auto returnValue = mysql_stmt_bind_named_param( m_mysqlStatementStruct, mysqlBindArray, m_bindVariablesCount, mysqlNamed );

        delete [] mysqlBindArray;
        delete [] mysqlNamed;

        return returnValue;

    }

}
