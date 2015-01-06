#ifndef OBJECT_CACHE__H_
#define OBJECT_CACHE__H_

#include <boost/shared_ptr.hpp>

template <typename T>
class ObjectCache {
public:
    boost::shared_ptr<T> Get( const std::string &name ) {
        if ( m_cache.find( name ) == m_cache.end() ) {
            boost::shared_ptr<T> v = Setup( name );
            if ( v ) {
                m_cache.insert( std::make_pair( name, v ) );
            }
        }
        return m_cache[ name ];
    }

    boost::shared_ptr<T> Setup( const std::string &name ) {
        boost::shared_ptr<T> v( new T());
        if ( !v->Init( name ) ) {
            return boost::shared_ptr<T>();
        }
        return v;
    }

private:
    std::map<std::string, boost::shared_ptr<T> > m_cache;
};

#endif /* OBJECT_CACHE__H_ */
