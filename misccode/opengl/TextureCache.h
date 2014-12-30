
#include <boost/shared_ptr.hpp>

template <typename T>
class ObjectCache {
public:
    boost::shared_ptr<T> Get( const std::string &name ) {
        if ( m_cache.find( name ) == m_cache.end() ) {
            boost::shared_ptr<T> v( new T());
            if ( !v->Init( name ) ) {
                return NULL;
            }
            m_cache.insert( std::make_pair( name, v ) );
        }
        return m_cache[ name ];
    }

private:
    std::map<std::string, boost::shared_ptr<T> > m_cache;
};
