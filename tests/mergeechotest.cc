#include "doctest/doctest.h"
#include "sharedstate.hh"
#include "shared_state_error_code.hh"
#include "piped_async_command.hh"

// Tests that don't naturally fit in the headers/.cpp files directly
// can be placed in a tests/*.cpp file. Integration tests are a good example.

TEST_CASE("returnmerge")
{
  std::string original = "mensajeaverificar";
  std::string merged = SharedState::mergestate(original);
  std::cout << merged << original << "--------------------------------------------------------------"<< std::endl;
  CHECK(original.size() == merged.size());
  CHECK(original == merged);
}

TEST_CASE("parametrizedmerge")
{
  std::string original = "mensajeaverificar";
  std::string merged;
  SharedState::mergestate(original, merged);
  std::cout << merged << original;
  CHECK(original.size() == merged.size());
  CHECK(original == merged);
}



void verificar(std::string original)
{
  auto merged = SharedState::mergestate(original);
  CHECK(original.size() == merged.size());
  CHECK(original == merged);
}

void verificarOptional(std::string original)
{
  auto merged = SharedState::optMergeState(original);
  CHECK(original.size() == merged.value().size());
  CHECK(original == merged.value());
}

void verificarExpected(std::string original)
{
  auto merged = SharedState::expMergestate(original);
  CHECK(original.size() == merged.value().size());
  CHECK(original == merged.value());
}

void verificarExpectedWillFail(std::string original)
{
  auto merged = SharedState::expMergestate(original,true);
  CHECK_FALSE(merged);
  std::error_condition(SharedState::SharedStateErrorCode::OpenPipeError);
  CHECK(merged.error().message() == make_error_condition(SharedState::SharedStateErrorCode::OpenPipeError).message());
}

// void verificarPiped(std::string original)
// {
//   IOContext io_context{};
//   char socbuffer[256] = {0};
//   std::unique_ptr<PipedAsyncCommand> asyncecho = std::make_unique<PipedAsyncCommand>("cat",&io_context);
//   asyncecho.get()->writepipe(original.data(),original.length());
//   asyncecho.get()->readpipe(socbuffer,256);
//   std::string merged(socbuffer);
//   std::cout << merged << " --- " << original;
//   CHECK(original.size() == merged.size());
//   CHECK(original == merged);
 
// }

TEST_CASE("Opt merge")
{
  std::string original = "mensajeaverificar";
  verificarOptional(original);
}

TEST_CASE("Parametrized merge test") {
    std::vector<std::string> data {"","mensajeaverificar", "asdasdaasd > saddsdfsdf","546654654654","546654654654546654654654546654654654546654654654" };

    for(auto& i : data) {
        CAPTURE(i); // log the current input data
        verificarOptional(i);
        verificar(i);
        verificarExpected(i);
        verificarExpectedWillFail(i);
        //verificarPiped(i);
    }
}

// sync vacio devuelve lo mismo que get
 