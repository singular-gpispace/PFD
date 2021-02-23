#include <string>
#include <vector>
#include <optional>

#include <installation.hpp>

#include <drts/client.hpp>
#include <drts/drts.hpp>
#include <drts/scoped_rifd.hpp>

#include <util-generic/executable_path.hpp>
#include <util-generic/print_exception.hpp>
#include <util-generic/read_lines.hpp>

#include "Singular/libsingular.h"

#include "parallel/singular_functions.hpp"
#include <share/include/type_aliases.hpp>

#define get_singular_string_argument(A,B,C) \
    require_argument<B, char*> (A,STRING_CMD,"string",C)

#define get_singular_int_argument(A,B,C) \
    require_argument<B, long> (A,INT_CMD,"int",C)

#define get_singular_lists_argument(A,B,C) \
    require_argument<B, lists> (A,LIST_CMD,"list",C)


namespace
{
  template<std::size_t arg_index>
  leftv to_nth_arg (leftv args)
  {
    for (std::size_t index (arg_index); args && index > 0; --index)
    {
      args = args->next;
    }

    return args ? args : throw std::invalid_argument ("too few arguments");
  }

  template<std::size_t arg_index, typename T>
  T require_argument (leftv args, int type, std::string type_name,
    std::string argument_name)
  try
  {
    leftv arg (to_nth_arg<arg_index> (args));
    if (arg->Typ() != type)
    {
      throw std::invalid_argument ("expected " + type_name);
    }
    return reinterpret_cast<T> (arg->Data());
  }
  catch (...)
  {
    std::throw_with_nested
      ( std::invalid_argument
         ( "argument " + std::to_string (arg_index)
         + " '" + argument_name + "'"
         )
      );
  }

  template<std::size_t first_optional>
    std::vector<std::string> remaining_string_arguments (leftv args)
  {
    std::vector<std::string> strings;

    leftv it (to_nth_arg<first_optional - 1> (args)->next);
    while (it)
    {
      if (it->Typ() != STRING_CMD)
      {
        throw std::invalid_argument ("expected string");
      }
      strings.emplace_back (reinterpret_cast<char*> (it->Data()));
      //std::cout << "extra arg: " << strings.back() << "\n";

      it = it->next;
    }

    return strings;
  }

  template<std::size_t arg_index, typename T>
    T nth_list_arg (lists l)
  {
    return reinterpret_cast<T> (l->m[arg_index].data);
  }

  class ArgumentState {
    public:
      ArgumentState (leftv args, std::string graph_type);

      int outToken() const;
      int procsPerNode() const;
      std::size_t numTasks() const;

      std::string tmpDir() const;
      std::string nodeFile() const;
      std::string showStrategy() const;
      std::string baseFileName() const;
      std::string inStructName() const;
      std::string inStructDesc() const;
      std::string outStructName() const;
      std::string outStructDesc() const;
      std::string neededLibrary() const;
      std::string functionName() const;
      std::string expandNodeFunction() const;
      std::string graphType() const;

      singular_parallel::installation singPI() const;
      lists argList() const;
      lists addArgsList() const;
    private:
      lists arg_list;
      std::string tmpdir;

      /* gc.options.{*} */
      std::string nodefile;
      int procspernode;
      std::string strategy;
      lists addargs_list;

      /* pc.options.{*} */
      std::string instructname;
      std::string instructdesc;
      std::string outstructname;
      std::string outstructdesc;
      std::string neededlibrary;
      std::string functionname;
      std::string expandnodefunction;

      std::string graph_type;

      /* derived variables */
      std::size_t num_tasks;
      int out_token;
      std::string base_filename;

      singular_parallel::installation singular_parallel_installation;
  };

  /*** private function declarations ***/
  int fetch_token_value_from_sing_scope (std::string token_s);
  int get_num_tasks(leftv args, lists arg_list, std::string graph_type);
  std::string get_base_filename(std::string const& graph_type,
                                std::string const& tmpdir);
  std::string get_expandnodefunction(leftv args, std::string graph_type);

  /*** ArgumentState implementations ***/
  int ArgumentState::outToken() const {
    return out_token;
  }

  int ArgumentState::procsPerNode() const {
    return procspernode;
  }

  std::size_t ArgumentState::numTasks() const {
    return num_tasks;
  }

  std::string ArgumentState::tmpDir() const {
    return tmpdir;
  }

  std::string ArgumentState::nodeFile() const {
    return nodefile;
  }

  std::string ArgumentState::showStrategy() const {
    return strategy;
  }

  std::string ArgumentState::baseFileName() const {
    return base_filename;
  }

  std::string ArgumentState::inStructName() const {
    return instructname;
  }

  std::string ArgumentState::inStructDesc() const {
    return instructdesc;
  }

  std::string ArgumentState::outStructName() const {
    return outstructname;
  }

  std::string ArgumentState::outStructDesc() const {
    return outstructdesc;
  }

  std::string ArgumentState::neededLibrary() const {
    return neededlibrary;
  }

  std::string ArgumentState::functionName() const {
    return functionname;
  }

  std::string ArgumentState::expandNodeFunction() const {
    return expandnodefunction;
  }

  std::string ArgumentState::graphType() const {
    return graph_type;
  }

  singular_parallel::installation ArgumentState::singPI() const {
    return singular_parallel_installation;
  }

  lists ArgumentState::argList() const {
    return arg_list;
  }

  lists ArgumentState::addArgsList() const {
    return addargs_list;
  }

  ArgumentState::ArgumentState (leftv args, std::string graph_type)
  : arg_list (get_singular_lists_argument(args, 0, "list of input structs"))
  , tmpdir (get_singular_string_argument(args, 1, "temp directory"))
  , nodefile (get_singular_string_argument(args, 2, "nodefile"))
  , procspernode (get_singular_int_argument(args, 3, "procs per node"))
  , strategy (get_singular_string_argument(args, 4, "rif strategy"))
  , addargs_list (get_singular_lists_argument(args, 5, "additional arguments"))
  , instructname (get_singular_string_argument(args, 6, "input struct name"))
  , instructdesc (get_singular_string_argument(args, 7, "input struct description"))
  , outstructname (get_singular_string_argument(args, 8, "output struct name"))
  , outstructdesc (get_singular_string_argument(args, 9, "output struct description"))
  , neededlibrary (get_singular_string_argument(args, 10, "needed library"))
  , functionname (get_singular_string_argument(args, 11, "function name"))
  , expandnodefunction (get_expandnodefunction(args, graph_type) )
  , graph_type (graph_type)
  , num_tasks (get_num_tasks(args, arg_list, graph_type))
  , out_token (fetch_token_value_from_sing_scope (outstructname))
  , base_filename (get_base_filename(graph_type, tmpdir))
  , singular_parallel_installation ()
  {
    if (out_token == 0)
    {
      throw std::invalid_argument ("struct does not exist for " + graph_type);
    }
  }

/*** private ArgState helper function implementations ***/

  int fetch_token_value_from_sing_scope (std::string token_s)
  {
    int token_v;
    blackboxIsCmd (token_s.c_str(), token_v);
    return token_v;
  }

  int get_num_tasks(leftv args, lists arg_list, std::string graph_type)
  {
    if ((graph_type == "list_all") || (graph_type == "list_first")) {
      return arg_list->nr + 1;
    } else if ((graph_type == "tree_all") || (graph_type == "rgraph_all")) {
      return (int)require_argument<13, long>
                    (args, INT_CMD, "int", "upper bound");
    } else {
      return 0;
    }
  }

  std::string get_base_filename(std::string const& graph_type,
                                std::string const& tmpdir)
  {
    std::string base_filename;
    if ((graph_type == "list_all") || (graph_type == "list_first")) {
      base_filename = tmpdir + "/sggspc_list";
    } else if (graph_type == "tree_all") {
      base_filename = tmpdir + "/sggspc_tree";
    } else if (graph_type == "rgraph_all") {
      base_filename = tmpdir + "/sggspc_rgraph";
    } else {
      throw std::runtime_error( std::string("Bad graph type as argument")
                              + "while determining base filename");
    }

    return base_filename;
  }

  std::string get_expandnodefunction(leftv args, std::string graph_type)
  {
    if ((graph_type == "tree_all") || (graph_type == "rgraph_all")) {
      return require_argument<12, char*> (args,
                                          STRING_CMD,
                                          "string",
                                          "expand node function");
    } else {
      return "";
    }
  }
}

/*** General helper function declarations ***/

void sggspc_print_current_exception (std::string s);
singular_parallel::pnet_list get_index_list(unsigned long count);
std::optional<std::multimap<std::string, pnet::type::value::value_type>>
    get_values_on_ports(ArgumentState const &as,
                        boost::filesystem::path const implementation);
std::optional<std::multimap<std::string, pnet::type::value::value_type>>
    gpis_launch_with_workflow (boost::filesystem::path workflow,
                               ArgumentState const &as);

/*** Library function declarations ***/
BOOLEAN sggspc_wait_all (leftv res, leftv args);
BOOLEAN sggspc_wait_first (leftv res, leftv args);
BOOLEAN sggspc_tree_wait_all (leftv res, leftv args);
BOOLEAN sggspc_rgraph_wait_all (leftv res, leftv args);

/*** General helper function implementation ***/

void sggspc_print_current_exception (std::string s)
{
  WerrorS (("singular_parallel: (" + s + ") " +
            fhg::util::current_exception_printer (": ").string()).c_str());
}


singular_parallel::pnet_list get_index_list(unsigned long count)
{
  singular_parallel::pnet_list l;
  for (unsigned long i = 0; i < count; i++ ) {
    l.push_back(i);
  }
  return l;
}


std::optional<std::multimap<std::string, pnet::type::value::value_type>>
    get_values_on_ports(ArgumentState const &as,
                        boost::filesystem::path const implementation)
{
  if ((as.graphType() == "list_all") || (as.graphType() == "list_first")) {
    std::multimap<std::string, pnet::type::value::value_type> values_on_ports
      ( { {"implementation", implementation.string()}
        , {"path_to_libsingular", fhg::util::executable_path (&siInit)
                                                              .parent_path()
                                                              .string()}
        , {"base_filename", as.baseFileName()}
        , {"function_name", as.functionName()}
        , {"in_struct_name", as.inStructName()}
        , {"in_struct_desc", as.inStructDesc()}
        , {"out_struct_name", as.outStructName()}
        , {"out_struct_desc", as.outStructDesc()}
        , {"needed_library", as.neededLibrary()}
        , {"task_count", static_cast<unsigned int> (as.numTasks())}
        }
      );
    return values_on_ports;
  } else if ((as.graphType() == "tree_all") ||
             (as.graphType() == "rgraph_all")) {
    singular_parallel::pnet_list l = get_index_list((unsigned long)as
                                                        .argList()
                                                        ->nr + 1);
    /*
    */
    std::multimap<std::string, pnet::type::value::value_type> values_on_ports
      ( { {"implementation", implementation.string()}
        , {"path_to_libsingular", fhg::util::executable_path (&siInit)
                                                              .parent_path()
                                                              .string()}
        , {"base_filename", as.baseFileName()}
        , {"function_name", as.functionName()}
        , {"expand_node_function", as.expandNodeFunction()}
        , {"in_struct_name", as.inStructName()}
        , {"in_struct_desc", as.inStructDesc()}
        , {"out_struct_name", as.outStructName()}
        , {"out_struct_desc", as.outStructDesc()}
        , {"needed_library", as.neededLibrary()}
        , {"max_count", static_cast<unsigned long> (as.numTasks())}
        , {"initial_task_list", l}
        }
      );
    return values_on_ports;
  } else {
    return std::nullopt;
  }
}

std::optional<std::multimap<std::string, pnet::type::value::value_type>>
                    gpis_launch_with_workflow (boost::filesystem::path workflow,
                    ArgumentState const &as)
try
{
  std::string debugout = as.tmpDir() + " " + as.nodeFile() + " " +
    std::to_string (as.procsPerNode()) + " " + as.showStrategy() + "\n" +
    as.inStructName() + " " + as.inStructDesc() + " " +
    as.outStructName() + " " + as.outStructDesc() + " " +
    as.neededLibrary() + " " + as.functionName() + " " +
    as.graphType() + "\n";

  std::vector<std::string> options;
  std::size_t num_addargs = as.addArgsList()->nr + 1;
  PrintS ((std::to_string (num_addargs) + " additional arguments\n").c_str());
  for (std::size_t i = 0; i < num_addargs; ++i)
  {
    int arg_type = as.addArgsList()->m[i].rtyp;
    if (arg_type != STRING_CMD)
    {
      throw std::invalid_argument ("wrong type of additional option "
        + std::to_string (i) + ", expected string got "
        + std::to_string (arg_type));
    }
    const std::string addarg_str
      (static_cast<char*> (as.addArgsList()->m[i].data));
    debugout += addarg_str + "\n";
    options.push_back (addarg_str);
  }

  PrintS (debugout.c_str());
  PrintS (("have " + std::to_string (as.numTasks()) + " tasks\n").c_str());

  int in_token, out_token;
  blackboxIsCmd ((as.inStructName()).c_str(), in_token);
  blackboxIsCmd ((as.outStructName()).c_str(), out_token);

  if ((in_token == 0) || (out_token == 0))
  {
    throw std::invalid_argument ("struct does not exist");
  }

  write_in_structs_to_file( as.argList()
                          , as.baseFileName()
                          , in_token
                          );

  // now prepare startup of GPI-Space
  // TODO: do this independent from actual call?

  std::string const topology_description
    ("worker:" + std::to_string (as.procsPerNode())); // here, only one type

  boost::filesystem::path const implementation
    (as.singPI().workflow_dir() /
     "libparallel_implementation.so");

  boost::program_options::options_description options_description;
  options_description.add_options() ("help", "Display this message");
  options_description.add (gspc::options::logging());
  options_description.add (gspc::options::scoped_rifd
                                      (gspc::options::rifd::rif_port));
  options_description.add (gspc::options::drts());

  boost::program_options::variables_map vm;
  boost::program_options::store
    ( boost::program_options::command_line_parser (options)
    . options (options_description).run()
    , vm
    );

  // help option left as is. If this is really to be used, it should be checked
  // earlier. Using std::cout is not really right, should rather write to
  // std::stringstream and use PrintS ...
  if (vm.count ("help"))
  {
    std::cout << options_description;
    return std::nullopt;
  }

  gspc::installation const gspc_installation (as.singPI()
                                                .gspc_installation (vm));

  gspc::scoped_rifds const scoped_rifd
    ( gspc::rifd::strategy
        { [&]
          {
            using namespace boost::program_options;
            variables_map vm;
            vm.emplace ("rif-strategy"
                       , variable_value (as.showStrategy(), false));
            vm.emplace ( "rif-strategy-parameters"
                       , variable_value (std::vector<std::string>{}, true)
                       );
            return vm;
          }()
        }
    , gspc::rifd::hostnames
        { [&]
          {
            try
            {
              return fhg::util::read_lines (as.nodeFile());
            }
            catch (...)
            {
              std::throw_with_nested (std::runtime_error ("reading nodefile"));
            }
          }()
        }
    , gspc::rifd::port {vm}
    , gspc_installation
    );

  gspc::scoped_runtime_system drts ( vm
                                   , gspc_installation
                                   , topology_description
                                   , scoped_rifd.entry_points()
                                   );

  auto values_on_ports = get_values_on_ports(as, implementation);
  if (!values_on_ports.has_value()) {
    return std::nullopt;
  }
  std::multimap<std::string, pnet::type::value::value_type> result
    ( gspc::client (drts).put_and_run
      ( gspc::workflow (workflow)
      , values_on_ports.value()
      )
    );
  return result;
}
catch (...)
{
  // need to check which resources must be tidied up
  sggspc_print_current_exception (std::string ("in gpis_launch_with_workflow"));
  return std::nullopt;
}

/*** Module initialization ***/

extern "C" int mod_init (SModulFunctions* psModulFunctions)
{
  // TODO: explicit check if module has already been loaded?
  //PrintS ("DEBUG: in mod_init\n");

  /*** lists ***/
  psModulFunctions->iiAddCproc
    ((currPack->libname ? currPack->libname : ""),
      "sggspc_wait_all", FALSE, sggspc_wait_all);

  psModulFunctions->iiAddCproc
    ((currPack->libname ? currPack->libname : ""),
      "sggspc_wait_first", FALSE, sggspc_wait_first);

  /*** trees ***/
  psModulFunctions->iiAddCproc
    ((currPack->libname ? currPack->libname : ""),
      "sggspc_tree_wait_all", FALSE, sggspc_tree_wait_all);

  /*** graphs ***/
  psModulFunctions->iiAddCproc
    ((currPack->libname ? currPack->libname : ""),
      "sggspc_rgraph_wait_all", FALSE, sggspc_rgraph_wait_all);

  return MAX_TOK;
}

/*** Library function implementation ***/

/*** lists ***/

BOOLEAN sggspc_wait_all (leftv res, leftv args)
try {

  ArgumentState as (args, "list_all");

  auto result = gpis_launch_with_workflow (as.singPI().workflow_all(), as);
  if (!result.has_value()) {
    res->rtyp = NONE;
    return FALSE;
  }

  std::multimap<std::string, pnet::type::value::value_type>::const_iterator
    sm_result_it (result.value().find ("output"));
  if (sm_result_it == result.value().end())
  {
    throw std::runtime_error ("Petri net has not finished correctly");
  }

  lists out_list = static_cast<lists> (omAlloc0Bin (slists_bin));
  out_list->Init (as.numTasks());

  for (std::size_t i = 0; i < as.numTasks(); i++)
  {
    si_link l = ssi_open_for_read(get_out_struct_filename(as.baseFileName(), i));
    // later consider case of "wrong" output (and do not throw)
    lists entry = ssi_read_newstruct (l, as.outStructName());
    ssi_close_and_remove (l);
    out_list->m[i].rtyp = as.outToken();
    out_list->m[i].data = entry;
  }


  res->rtyp = LIST_CMD;
  res->data = out_list;

  return FALSE;
}
catch (...)
{
  // need to check which resources must be tidied up
  sggspc_print_current_exception (std::string ("in sggspc_wait_all"));
  return TRUE;
}

BOOLEAN sggspc_wait_first (leftv res, leftv args)
try
{
  ArgumentState as (args, "list_first");

  auto result = gpis_launch_with_workflow (as.singPI().workflow_first(), as);
  if (!result.has_value()) {
    res->rtyp = NONE;
    return FALSE;
  }

  std::multimap<std::string, pnet::type::value::value_type>::const_iterator
                          sm_result_it (result.value().find ("output"));
  if (sm_result_it == result.value().end())
  {
    throw std::runtime_error ("Petri net has not finished correctly");
  }
  unsigned int i = boost::get<unsigned int> (sm_result_it->second);

  si_link l = ssi_open_for_read (as.baseFileName() +
                                 ".o" +
                                 std::to_string (i));

  lists entry = ssi_read_newstruct (l, as.outStructName());
  ssi_close_and_remove (l);

  res->rtyp = as.outToken();
  res->data = entry;

  return FALSE;
}
catch (...)
{
  // need to check which resources must be tidied up
  sggspc_print_current_exception (std::string ("in sggspc_wait_first"));
  return TRUE;
}

/*** trees ***/

BOOLEAN sggspc_tree_wait_all (leftv res, leftv args)
try {

  ArgumentState as (args, "tree_all");

  auto result = gpis_launch_with_workflow (as.singPI().workflow_tree_all(), as);
  if (!result.has_value()) {
    res->rtyp = NONE;
    return FALSE;
  }

  std::multimap<std::string, pnet::type::value::value_type>::const_iterator
    sm_result_it (result.value().find ("output"));
  if (sm_result_it == result.value().end())
  {
    throw std::runtime_error ("Petri net has not finished correctly");
  }
  if (result.value().count("computed_node_count") != 1) {
    throw std::runtime_error ("No computed node count returned");
  }

  pnet::type::value::value_type const pn_computed_node_count
      (result.value().find ("computed_node_count")->second);
  unsigned long computed_node_count = boost::get<unsigned long> 
                                          (pn_computed_node_count);

  std::cout << std::to_string(computed_node_count) << " nodes computed.\n";

  lists out_list = static_cast<lists> (omAlloc0Bin (slists_bin));
  out_list->Init (computed_node_count);

  for (std::size_t i = 0; i < computed_node_count; i++)
  {
    si_link l = ssi_open_for_read (as.baseFileName() +
                                   ".o" +
                                   std::to_string (i));
    // later consider case of "wrong" output (and do not throw)
    lists entry = ssi_read_newstruct (l, as.outStructName());
    ssi_close_and_remove (l);
    out_list->m[i].rtyp = as.outToken();
    out_list->m[i].data = entry;
  }


  res->rtyp = LIST_CMD;
  res->data = out_list;

  return FALSE;
}
catch (...)
{
  // need to check which resources must be tidied up
  sggspc_print_current_exception (std::string ("in sggspc_tree_wait_all"));
  return TRUE;
}

/*** graphs ***/

BOOLEAN sggspc_rgraph_wait_all (leftv res, leftv args)
try {

  ArgumentState as (args, "rgraph_all");

  auto result = gpis_launch_with_workflow (as.singPI().workflow_rgraph_all(), as);
  if (!result.has_value()) {
    res->rtyp = NONE;
    return FALSE;
  }

  std::multimap<std::string, pnet::type::value::value_type>::const_iterator
    sm_result_it (result.value().find ("output"));
  if (sm_result_it == result.value().end())
  {
    throw std::runtime_error ("Petri net has not finished correctly");
  }
  if (result.value().count("computed_node_count") != 1) {
    throw std::runtime_error ("No computed node count returned");
  }

  pnet::type::value::value_type const pn_computed_node_count
      (result.value().find ("computed_node_count")->second);
  unsigned long computed_node_count = boost::get<unsigned long> 
                                          (pn_computed_node_count);

  std::cout << std::to_string(computed_node_count) << " nodes computed.\n";

  lists out_list = static_cast<lists> (omAlloc0Bin (slists_bin));
  out_list->Init (computed_node_count);

  for (std::size_t i = 0; i < computed_node_count; i++)
  {
    si_link l = ssi_open_for_read (as.baseFileName() +
                                   ".o" +
                                   std::to_string (i));
    // later consider case of "wrong" output (and do not throw)
    lists entry = ssi_read_newstruct (l, as.outStructName());
    ssi_close_and_remove (l);
    out_list->m[i].rtyp = as.outToken();
    out_list->m[i].data = entry;
  }


  res->rtyp = LIST_CMD;
  res->data = out_list;

  return FALSE;
}
catch (...)
{
  // need to check which resources must be tidied up
  sggspc_print_current_exception (std::string ("in sggspc_rgraph_wait_all"));
  return TRUE;
}