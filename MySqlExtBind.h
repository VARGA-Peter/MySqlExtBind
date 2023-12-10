/**
 * FaFMySqlStmtBindNamedParam.h
 *
 * Header for the FaFMySqlStmtBindNamedParam class.
 *
 * Written by Peter VARGA
 * Created 2023-12-09
 *
 * Version 1.00
 *
 */

#include <algorithm>
#include <cstring>
#include <iostream>
#include <map>
#include <regex>
#include <stdexcept>
#include <string>

#include <mysql.h>

namespace FaF
{

    /**
     * The position in the MySQL command must be saved for each MYSQL_BIND item in order to create the final MYSQL_BIND array
     * in the correct order according to the provided bind names.
     * This type is the value in each std::map.
     */
    using MapItem = struct MapItem
    {

            u_int       bindNamePosition;
            bool        assignBindData {};
            MYSQL_BIND  mysqlBindItem  {};

    };

    using MapContainer = std::map< const std::string, MapItem >;        // @suppress("Invalid template arguments")

    class MySqlExtBind
    {

        private:

            auto parseMysqlCommand()                                                                            -> void;
            auto copyBindStructure( const std::string bindVariable, const MYSQL_BIND & sourceBindStructure )    -> void;

            // constructor initialiser list - respect the order.

                /**
                 * The pointer to the MySQL statement - mysql_stmt_init() must have been already called, otherwise undefined behaviour.
                 * Do not call mysql_stmt_prepare() as it contains syntax errors due to the extended bind name format.
                 */
                MYSQL_STMT *        m_mysqlStatementStruct;
                const std::string   m_mysqlCommand;
                std::string         m_adjustedMysqlCommand;

            // Flag if the regex check has been done to assure the delimiters are correctly recognised.
            static bool             m_regexHasBeenChecked;

            // The left and right delimiter - can be overwritten any time using the static function ::setDelimiters()
            static std::string      m_leftDelimiter;
            static std::string      m_rightDelimiter;

            // This container maps to each named bind variable the original MYSQL_BIND structure and the position in the SQL statement.
            MapContainer            m_bindNamesContainer {};

            // The MYSQL_BIND array which will be filled in the correct order once all bind variables have been processed.
            MYSQL_BIND *            m_finalMysqlBindArray;

            // How many bind variables does this statement have.
            u_int                   m_bindVariablesCount {};

        public:

            MySqlExtBind( MYSQL_STMT * _mysqlStatementStruct, const std::string _mysqlCommand );

            auto assignBindData( const std::string, const MYSQL_BIND & originalMysqlBindItem ) -> void;
            auto assignBindData(
                    const std::string,
                    // m_finalMysqlBindArray is only a placeholder so we can access the MYSQL_BIND's members.
                    decltype( m_finalMysqlBindArray->buffer_type ),
                    decltype( m_finalMysqlBindArray->buffer      ),
                    decltype( m_finalMysqlBindArray->length      ) length  = nullptr,
                    decltype( m_finalMysqlBindArray->is_null     ) is_null = nullptr
            ) -> void;
            auto executeBind()      -> decltype( mysql_stmt_bind_named_param( nullptr, nullptr, 0, nullptr ) );
            auto prepareStatement() -> decltype( mysql_stmt_prepare( nullptr, nullptr, 0 ) );

            static auto setDelimiters( const std::string leftDelimiter = ":", const std::string rightDelimiter = "" ) -> void;

    };

    class Exception : public std::exception
    {
    };

}
