/*=========================================================================

  Program:   Insight Segmentation & Registration Toolkit
  Module:    $RCSfile$
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

  Copyright (c) 2002 Insight Consortium. All rights reserved.
  See ITKCopyright.txt or http://www.itk.org/HTML/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "cmTarget.h"
#include "cmMakefile.h"

#include <map>
#include <set>


void cmTarget::GenerateSourceFilesFromSourceLists( cmMakefile &mf)
{
  // this is only done for non install targets
  if ((this->m_TargetType == cmTarget::INSTALL_FILES)
      || (this->m_TargetType == cmTarget::INSTALL_PROGRAMS))
    {
    return;
    }

  // for each src lists add the classes
  for (std::vector<std::string>::const_iterator s = m_SourceLists.begin();
       s != m_SourceLists.end(); ++s)
    {
    // replace any variables
    std::string temps = *s;
    mf.ExpandVariablesInString(temps);
    // look for a srclist
    if (mf.GetSources().find(temps) != mf.GetSources().end())
      {
      const std::vector<cmSourceFile*> &clsList = 
        mf.GetSources().find(temps)->second;
      // if we ahave a limited build list, use it
      m_SourceFiles.insert(m_SourceFiles.end(), 
                           clsList.begin(), 
                           clsList.end());
      }
    // if one wasn't found then assume it is a single class
    else
      {
      cmSourceFile file;
      file.SetIsAnAbstractClass(false);
      file.SetName(temps.c_str(), mf.GetCurrentDirectory(),
                   mf.GetSourceExtensions(),
                   mf.GetHeaderExtensions());
      m_SourceFiles.push_back(mf.AddSource(file));
      }
    }

  // expand any link library variables whle we are at it
  LinkLibraries::iterator p = m_LinkLibraries.begin();
  for (;p != m_LinkLibraries.end(); ++p)
    {
    mf.ExpandVariablesInString(p->first);
    }
}

void cmTarget::MergeLibraries(const LinkLibraries &ll)
{
  m_LinkLibraries.insert( m_LinkLibraries.end(), ll.begin(), ll.end() );
}

void cmTarget::MergeDirectories(const std::vector<std::string>  &ld)
{
  m_LinkDirectories.insert( m_LinkDirectories.end(), ld.begin(), ld.end() );
}


void cmTarget::AddLinkLibrary(const std::string& lib, 
                              LinkLibraryType llt)
{
  m_LinkLibraries.push_back( std::pair<std::string, cmTarget::LinkLibraryType>(lib,llt) );
}

void cmTarget::AddLinkLibrary(cmMakefile& mf,
                              const char *target, const char* lib, 
                              LinkLibraryType llt)
{
  m_LinkLibraries.push_back( std::pair<std::string, cmTarget::LinkLibraryType>(lib,llt) );

  mf.AddDependencyToCache( target, lib );
}

bool cmTarget::HasCxx() const
{
  for(std::vector<cmSourceFile*>::const_iterator i =  m_SourceFiles.begin();
      i != m_SourceFiles.end(); ++i)
    {
    if((*i)->GetSourceExtension() != "c")
      {
      return true;
      }
    }
  return false;
}





void
cmTarget::AnalyzeLibDependencies( const cmMakefile& mf )
{
  typedef std::map< std::string, std::pair<std::string,LinkLibraryType> > LibMap;
  typedef std::vector< std::string > LinkLine;

  // Maps the canonical names to the full objects of m_LinkLibraries.
  LibMap lib_map;

  // The list canonical names in the order they were orginally
  // specified on the link line (m_LinkLibraries).
  LinkLine lib_order;

  // The dependency maps.
  DependencyMap dep_map, dep_map_implicit;

  LinkLibraries::iterator lib;
  for(lib = m_LinkLibraries.begin(); lib != m_LinkLibraries.end(); ++lib)
    {
    // skip zero size library entries, this may happen
    // if a variable expands to nothing.
    if (lib->first.size() == 0) continue;

    std::string cname = CanonicalLibraryName(lib->first);
    lib_map[ cname ] = *lib;
    lib_order.push_back( cname );
    }

  // First, get the explicit dependencies for those libraries that
  // have specified them
  for( LibMap::iterator i = lib_map.begin(); i != lib_map.end(); ++i )
    {
    GatherDependencies( mf, i->first, dep_map );
    }

  // For the rest, get implicit dependencies. A library x depends
  // implicitly on a library y if x appears before y on the link
  // line. However, x does not implicitly depend on y if y
  // *explicitly* depends on x [*1]--such cyclic dependencies must be
  // explicitly specified. Note that implicit dependency cycles can
  // still occur: "-lx -ly -lx" will generate a implicit dependency
  // cycle provided that neither x nor y have explicit dependencies.
  //
  // [*1] This prevents external libraries from depending on libraries
  // generated by this project.

  for( LibMap::iterator i = lib_map.begin(); i != lib_map.end(); ++i )
    {
    if( dep_map.find( i->first ) == dep_map.end() )
      {
      LinkLine::iterator pos = std::find( lib_order.begin(), lib_order.end(), i->first );
      for( ; pos != lib_order.end(); ++pos )
        {
        std::set<std::string> visited;
        if( !DependsOn( *pos, i->first, dep_map, visited ) )
          {
          dep_map_implicit[ i->first ].insert( *pos );
          }
        }
      dep_map_implicit[ i->first ].erase( i->first ); // cannot depend on itself
      }
    }

  // Combine all the depedency information
  //   dep_map.insert( dep_map_implicit.begin(), dep_map_implicit.end() );
  // doesn't work under MSVC++.
  for( DependencyMap::iterator i = dep_map_implicit.begin();
       i != dep_map_implicit.end(); ++i )
	{
	dep_map[ i->first ] = i->second;
	}

  // Create a new link line order.
  std::set<std::string> done, visited;
  std::vector<std::string> link_line;
  for( LibMap::iterator i = lib_map.begin(); i != lib_map.end(); ++i )
    {
    Emit( i->first, dep_map, done, visited, link_line );
    }


  // If LIBRARY_OUTPUT_PATH is not set, then we must add search paths
  // for all the new libraries added by the dependency analysis.
  const char* libOutPath = mf.GetDefinition("LIBRARY_OUTPUT_PATH");
  bool addLibDirs = (libOutPath==0 || strcmp(libOutPath,"")==0);

  m_LinkLibraries.clear();
  for( std::vector<std::string>::reverse_iterator i = link_line.rbegin();
       i != link_line.rend(); ++i )
    {
    // Some of the libraries in the new link line may not have been in
    // the orginal link line, but were added by the dependency
    // analysis. For these libraries, we assume the GENERAL type and
    // add the name of the library.
    if( lib_map.find(*i) == lib_map.end() )
      {
      if( addLibDirs )
        {
        const char* libpath = mf.GetDefinition( i->c_str() );
        if( libpath )
          {
          // Don't add a link directory that is already present.
          if(std::find(m_LinkDirectories.begin(),
                       m_LinkDirectories.end(), libpath) == m_LinkDirectories.end())
            {
            m_LinkDirectories.push_back(libpath);
            }
          }
        }
      m_LinkLibraries.push_back( std::make_pair(*i,GENERAL) );
      }
    else
      {
      m_LinkLibraries.push_back( lib_map[ *i ] );
      }
    }
}



std::string cmTarget::CanonicalLibraryName( const std::string& lib ) const
{
  cmRegularExpression reg("((^[ \t]*\\-l)|(^[ \t]*\\-framework[ \t]*))(.+)");
  if(lib.find('/') != std::string::npos
     && !reg.find(lib))
    {
    std::string dir, file;
    cmSystemTools::SplitProgramPath(lib.c_str(),
                                      dir, file);
    cmRegularExpression libname("lib(.*)(\\.so|\\.sl|\\.a|\\.dylib).*");
    cmRegularExpression libname_noprefix("(.*)(\\.so|\\.sl|\\.a|\\.dylib|\\.lib).*");
    if(libname.find(file))
      {
      return libname.match(1);
      }
    else if(libname_noprefix.find(file))
      {
      return libname_noprefix.match(1);
      }
	else
	  {
	  return file;
	  }
    }
  else
    {
    if(!reg.find(lib))
      {
      return lib;
      }
    else
      {
      return reg.match(4);
      }
    }
}


void cmTarget::Emit( const std::string& lib,
                     const DependencyMap& dep_map,
                     std::set<std::string>& emitted,
                     std::set<std::string>& visited,
                     std::vector<std::string>& link_line ) const
{
  // It's already been emitted
  if( emitted.find(lib) != emitted.end() )
    return;

  // If this library hasn't been visited before, then emit all its
  // dependencies before emitting the library itself. If it has been
  // visited before, then there is a dependency cycle. Just emit the
  // library itself, and let the recursion that got us here deal with
  // emitting the dependencies for the library.

  if( visited.insert(lib).second )
    {
    if( dep_map.find(lib) != dep_map.end() ) // does it have dependencies?
      {
      const std::set<std::string>& dep_on = dep_map.find( lib )->second;
      std::set<std::string>::const_iterator i;
      for( i = dep_on.begin(); i != dep_on.end(); ++i )
        {
        Emit( *i, dep_map, emitted, visited, link_line );
        }
      }
    }
  link_line.push_back( lib );
  emitted.insert(lib);
}


void cmTarget::GatherDependencies( const cmMakefile& mf,
                                   const std::string& lib,
                                   DependencyMap& dep_map ) const
{
  // If the library is already in the dependency map, then it has
  // already been fully processed.
  if( dep_map.find(lib) != dep_map.end() )
    return;

  const char* deps = mf.GetDefinition( (lib+"_LIB_DEPENDS").c_str() );
  if( deps )
    {
    // Make sure this library is in the map, even if it has an empty
    // set of dependencies. This distinguishes the case of explicitly
    // no dependencies with that of unspecified dependencies.
    dep_map[lib];

    // Parse the dependency information, which is simply a set of
    // libraries separated by ";". There is always a trailing ";".
    std::string depline = deps;
    std::string::size_type start = 0;
    std::string::size_type end;
    end = depline.find( ";", start );
    while( end != std::string::npos )
      {
      std::string l = depline.substr( start, end-start );
      if( l.size() != 0 )
        {
        const std::string cname = CanonicalLibraryName(l);
        dep_map[ lib ].insert( cname );
        GatherDependencies( mf, cname, dep_map );
        }
      start = end+1; // skip the ;
      end = depline.find( ";", start );
      }
    dep_map[lib].erase(lib); // cannot depend on itself
    }
}


bool cmTarget::DependsOn( const std::string& lib1, const std::string& lib2,
                          const DependencyMap& dep_map,
                          std::set<std::string>& visited ) const
{
  if( !visited.insert( lib1 ).second )
    return false; // already visited here

  if( lib1 == lib2 )
    return false;

  if( dep_map.find(lib1) == dep_map.end() )
    return false; // lib1 doesn't have any dependencies

  const std::set<std::string>& dep_set = dep_map.find(lib1)->second;

  if( dep_set.end() != dep_set.find( lib2 )  )
    return true; // lib1 doesn't directly depend on lib2.

  // Do a recursive check: does lib1 depend on x which depends on lib2?
  for( std::set<std::string>::const_iterator itr = dep_set.begin();
       itr != dep_set.end(); ++itr )
    {
      if( DependsOn( *itr, lib2, dep_map, visited ) )
        return true;
    }

  return false;
}
