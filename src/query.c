/*
    RedStore - a lightweight RDF triplestore powered by Redland
    Copyright (C) 2010-2011 Nicholas J Humfrey <njh@aelius.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define _POSIX_C_SOURCE 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "redstore.h"

static redhttp_response_t *perform_query(redhttp_request_t * request, const char *query_string)
{
  librdf_query *query = NULL;
  librdf_query_results *results = NULL;
  redhttp_response_t *response = NULL;
  const char *lang = redhttp_request_get_argument(request, "lang");

  if (lang == NULL)
    lang = DEFAULT_QUERY_LANGUAGE;

  redstore_debug("query_lang='%s'", lang);
  redstore_debug("query_string='%s'", query_string);

  query = librdf_new_query(world, lang, NULL, (unsigned char *) query_string, NULL);
  if (!query) {
    response = redstore_page_new_with_message(
      request, LIBRDF_LOG_ERROR, REDHTTP_INTERNAL_SERVER_ERROR,
      "There was an error while creating the query."
    );
    goto CLEANUP;
  }

  results = librdf_model_query_execute(model, query);
  if (!results) {
    response = redstore_page_new_with_message(
      request, LIBRDF_LOG_INFO, REDHTTP_INTERNAL_SERVER_ERROR,
      "There was an error while executing the query."
    );
    goto CLEANUP;
  }

  query_count++;

  if (librdf_query_results_is_bindings(results)) {
    response = format_bindings_query_result(request, results);
  } else if (librdf_query_results_is_graph(results)) {
    librdf_stream *stream = librdf_query_results_as_stream(results);
    if (stream) {
      response = format_graph_stream(request, stream);
      librdf_free_stream(stream);
    } else {
      response = redstore_page_new_with_message(
        request, LIBRDF_LOG_DEBUG, REDHTTP_INTERNAL_SERVER_ERROR, "Failed to get query results graph."
      );
    }
  } else if (librdf_query_results_is_boolean(results)) {
    response = format_bindings_query_result(request, results);
  } else if (librdf_query_results_is_syntax(results)) {
    response = redstore_page_new_with_message(
      request, LIBRDF_LOG_INFO, REDHTTP_NOT_IMPLEMENTED, "Syntax results format is not supported."
    );
  } else {
    response = redstore_page_new_with_message(
      request, LIBRDF_LOG_INFO, REDHTTP_INTERNAL_SERVER_ERROR, "Unknown librdf results type."
    );
  }


CLEANUP:
  if (results)
    librdf_free_query_results(results);
  if (query)
    librdf_free_query(query);

  return response;
}


static redhttp_response_t *perform_tpf(redhttp_request_t * request,
                                       const char *s_string, const char *p_string, const char *o_string)
{
  redhttp_response_t *response = NULL;

  librdf_node *s = NULL;
  if(s_string) {
    s = librdf_new_node_from_uri_string(world, s_string);
  }
  librdf_node *p = NULL;
  if(p_string) {
    p = librdf_new_node_from_uri_string(world, p_string);
  }
  librdf_node *o = NULL;
  if(o_string) {
    if(librdf_heuristic_object_is_literal(o_string)) {
      o = librdf_new_node_from_literal(world, o_string, NULL, 0);
    } else {
      o = librdf_new_node_from_uri_string(world, o_string);
    }

  }

  librdf_statement *statement = librdf_new_statement_from_nodes(world, s, p, o);

  librdf_stream *stream = librdf_model_find_statements(model,statement);

  if (stream) {
    response = format_graph_stream_as_ttl(request,stream);
//    response = format_graph_stream(request, stream);
    librdf_free_stream(stream);
  }

  if (s)
    librdf_free_node(s);
  if (p)
    librdf_free_node(p);
  if (o)
    librdf_free_node(o);

  return response;
}


redhttp_response_t *handle_query(redhttp_request_t * request, void *user_data)
{
  const char *query_string = NULL;

  // Do we have a query string?
  query_string = redhttp_request_get_argument(request, "query");
  if (query_string) {
    return perform_query(request, query_string);
  } else {
    return handle_page_query_form(request, user_data);
  }
}

redhttp_response_t *handle_tpf(redhttp_request_t * request, void *user_data)
{
  const char *s_string = NULL;
  const char *p_string = NULL;
  const char *o_string = NULL;

  s_string = redhttp_request_get_argument(request, "s");
  p_string = redhttp_request_get_argument(request, "p");
  o_string = redhttp_request_get_argument(request, "o");

  return perform_tpf(request, s_string, p_string, o_string);
}

redhttp_response_t *handle_sparql(redhttp_request_t * request, void *user_data)
{
  const char *method = redhttp_request_get_method(request);
  redhttp_response_t *response = NULL;
  const char *query_string = NULL;

  query_string = redhttp_request_get_argument(request, "query");
  if (query_string) {
    response = perform_query(request, query_string);
  } else if (strcmp(method, "GET")==0) {
    response = handle_description_get(request, user_data);
  } else {
    response = redstore_page_new_with_message(
      request, LIBRDF_LOG_INFO, REDHTTP_BAD_REQUEST, "Missing query string."
    );
  }

  return response;
}
